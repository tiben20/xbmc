/* 
 *  Copyright (C) 2003-2006 Gabest
 *  http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "ISubPic.h"

// CDX9SubPic

class CVirtualLock
{
public:
  virtual void Lock() = 0;
  virtual void Unlock() = 0;
};

typedef void (FLock)(void *_pLock);

class CScopeLock
{
  void *m_pLock;
  FLock *m_pUnlockFunc;
public:

  template <typename t_Lock>
  class TCLocker
  {
  public:
    static void fs_Locker(void *_pLock)
    {
      ((t_Lock *)_pLock)->Unlock();
    }
  };

  template <typename t_Lock>
  CScopeLock(t_Lock &_Lock)
  {
    _Lock.Lock();
    m_pLock = &_Lock;
    m_pUnlockFunc = TCLocker<t_Lock>::fs_Locker;
  }

  ~CScopeLock()
  {
    m_pUnlockFunc(m_pLock);
  }
};


class CDX9SubPicAllocator;
class CDX9SubPic : public ISubPicImpl
{
  Com::SComPtr<IDirect3DSurface9> m_pSurface;

protected:
  STDMETHODIMP_(void*) GetObject(); // returns IDirect3DTexture9*

public:
  CDX9SubPicAllocator *m_pAllocator;
  CDX9SubPic(IDirect3DSurface9* pSurface, CDX9SubPicAllocator *pAllocator);
  ~CDX9SubPic();

  // ISubPic
  STDMETHODIMP GetDesc(SubPicDesc& spd);
  STDMETHODIMP CopyTo(ISubPic* pSubPic);
  STDMETHODIMP ClearDirtyRect(DWORD color);
  STDMETHODIMP Lock(SubPicDesc& spd);
  STDMETHODIMP Unlock(RECT* pDirtyRect);
  STDMETHODIMP AlphaBlt(RECT* pSrc, RECT* pDst, SubPicDesc* pTarget);
  STDMETHODIMP GetTexture(Com::SComPtr<IDirect3DTexture9>& pTexture);
};

// CDX9SubPicAllocator

class CDX9SubPicAllocator : public ISubPicAllocatorImpl, public CCritSec
{
  IDirect3DDevice9* m_pD3DDev;
  Com::SmartSize m_maxsize;


  bool Alloc(bool fStatic, ISubPic** ppSubPic);
  void FreeTextures();

public:
  static CCritSec ms_SurfaceQueueLock;
  std::list<Com::SComPtrForList<IDirect3DSurface9>> m_FreeSurfaces;
  std::list<CDX9SubPic *> m_AllocatedSurfaces;

  void GetStats(int &_nFree, int &_nAlloc);

  CDX9SubPicAllocator(IDirect3DDevice9* pD3DDev, SIZE maxsize, bool fPow2Textures);
  ~CDX9SubPicAllocator();
  void ClearCache();

  // ISubPicAllocator
  STDMETHODIMP ChangeDevice(IUnknown* pDev);
  STDMETHODIMP SetMaxTextureSize(SIZE MaxTextureSize);
  STDMETHODIMP_(void) SetInverseAlpha(bool bInverted) {};
};
