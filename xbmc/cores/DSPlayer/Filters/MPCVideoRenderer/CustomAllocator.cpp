/*
* (C) 2018-2021 see Authors.txt
*
* This file is part of MPC-BE.
*
* MPC-BE is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* MPC-BE is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#include "stdafx.h"
#include "CustomAllocator.h"
#include "Helper.h"
#include "VideoRendererInputPin.h"

CCustomMediaSample::CCustomMediaSample(LPCTSTR pName, CBaseAllocator *pAllocator, HRESULT *phr, LPBYTE pBuffer, LONG length)
	: CMediaSampleSideData(pName, pAllocator, phr, pBuffer, length)
{
}

CCustomAllocator::CCustomAllocator(LPCTSTR pName, LPUNKNOWN pUnk, CVideoRendererInputPin* pVideoRendererInputPin, HRESULT *phr)
	: CMemAllocator(pName, nullptr, phr)
	, m_pVideoRendererInputPin(pVideoRendererInputPin)
{
}

CCustomAllocator::~CCustomAllocator()
{
	if (m_pVideoRendererInputPin && m_pVideoRendererInputPin->m_pCustomAllocator == this) {
		m_pVideoRendererInputPin->m_pCustomAllocator = nullptr;
	}

	SAFE_DELETE(m_pNewMT);
}

HRESULT CCustomAllocator::Alloc(void)
{
    CAutoLock lck(this);

    /* Check he has called SetProperties */
    HRESULT hr = CBaseAllocator::Alloc();
    if (FAILED(hr)) {
        return hr;
    }

    /* If the requirements haven't changed then don't reallocate */
    if (hr == S_FALSE) {
        ASSERT(m_pBuffer);
        return NOERROR;
    }
    ASSERT(hr == S_OK); // we use this fact in the loop below

    /* Free the old resources */
    if (m_pBuffer) {
        ReallyFree();
    }

    /* Make sure we've got reasonable values */
    if ( m_lSize < 0 || m_lPrefix < 0 || m_lCount < 0 ) {
        return E_OUTOFMEMORY;
    }

    /* Compute the aligned size */
    LONG lAlignedSize = m_lSize + m_lPrefix;

    /*  Check overflow */
    if (lAlignedSize < m_lSize) {
        return E_OUTOFMEMORY;
    }

    if (m_lAlignment > 1) {
        LONG lRemainder = lAlignedSize % m_lAlignment;
        if (lRemainder != 0) {
            LONG lNewSize = lAlignedSize + m_lAlignment - lRemainder;
            if (lNewSize < lAlignedSize) {
                return E_OUTOFMEMORY;
            }
            lAlignedSize = lNewSize;
        }
    }

    /* Create the contiguous memory block for the samples
       making sure it's properly aligned (64K should be enough!)
    */
    ASSERT(lAlignedSize % m_lAlignment == 0);

    LONGLONG lToAllocate = m_lCount * (LONGLONG)lAlignedSize;

    /*  Check overflow */
    if (lToAllocate > MAXLONG) {
        return E_OUTOFMEMORY;
    }

    m_pBuffer = (PBYTE)VirtualAlloc(NULL,
                    (LONG)lToAllocate,
                    MEM_COMMIT,
                    PAGE_READWRITE);

    if (m_pBuffer == NULL) {
        return E_OUTOFMEMORY;
    }

    LPBYTE pNext = m_pBuffer;
    CCustomMediaSample *pSample;

    ASSERT(m_lAllocated == 0);

    // Create the new samples - we have allocated m_lSize bytes for each sample
    // plus m_lPrefix bytes per sample as a prefix. We set the pointer to
    // the memory after the prefix - so that GetPointer() will return a pointer
    // to m_lSize bytes.
    for (; m_lAllocated < m_lCount; m_lAllocated++, pNext += lAlignedSize) {


        pSample = new(std::nothrow) CCustomMediaSample(
                            L"Custom media sample",
                            this,
                            &hr,
                            pNext + m_lPrefix,      // GetPointer() value
                            m_lSize);               // not including prefix

            ASSERT(SUCCEEDED(hr));
        if (pSample == NULL) {
            return E_OUTOFMEMORY;
        }

        // This CANNOT fail
        m_lFree.Add(pSample);
    }

    m_bChanged = FALSE;
    return NOERROR;
}

STDMETHODIMP CCustomAllocator::SetProperties(__in ALLOCATOR_PROPERTIES* pRequest, __out ALLOCATOR_PROPERTIES* pActual)
{
	CheckPointer(pActual, E_POINTER);
	ValidateReadWritePtr(pActual, sizeof(ALLOCATOR_PROPERTIES));

	ASSERT(pRequest->cbBuffer > 0);

	if (m_cbBuffer) {
		pRequest->cbBuffer = m_cbBuffer;
		m_cbBuffer = 0;
	}

	return __super::SetProperties(pRequest, pActual);
}

HRESULT CCustomAllocator::GetBuffer(IMediaSample** ppBuffer, REFERENCE_TIME* pStartTime, REFERENCE_TIME* pEndTime, DWORD dwFlags)
{
	HRESULT hr = __super::GetBuffer(ppBuffer, pStartTime, pEndTime, dwFlags);

	if (SUCCEEDED(hr) && m_pNewMT) {
		DLog(L"CCustomAllocator::GetBuffer() : Set new media type for MediaSample\n{}", MediaType2Str(m_pNewMT));
		(*ppBuffer)->SetMediaType(m_pNewMT);
		SAFE_DELETE(m_pNewMT);
		m_cbBuffer = 0;
	}

	return hr;
}

void CCustomAllocator::SetNewMediaType(const CMediaType& mt)
{
	DLog(L"CCustomAllocator::SetNewMediaType()");

	SAFE_DELETE(m_pNewMT);
	m_pNewMT = new CMediaType(mt);

	m_cbBuffer = 0;
	if (const auto pBIH = GetBIHfromVIHs(m_pNewMT); pBIH) {
		m_cbBuffer = pBIH->biSizeImage ? pBIH->biSizeImage : DIBSIZE(*pBIH);
	}
}

void CCustomAllocator::ClearNewMediaType()
{
	SAFE_DELETE(m_pNewMT);
}
