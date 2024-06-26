// This is a part of the Active Template Library.
// Copyright (C) Microsoft Corporation
// All rights reserved.
//
// This source code is only intended as a supplement to the
// Active Template Library Reference and related
// electronic documentation provided with the library.
// See these sources for detailed information regarding the    
// Active Template Library product.

#ifndef __SCOMCLI_H__
#define __SCOMCLI_H__

#pragma once

#include <unknwn.h>

#ifndef SASSERT
#include <assert.h>
#define SASSERT(x) assert(x);
#endif

#pragma warning (push)
#pragma warning (disable: 4127)  // conditional expression constant
#pragma warning (disable: 4510)  // compiler cannot generate default constructor
#pragma warning (disable: 4571)  //catch(...) blocks compiled with /EHs do NOT catch or re-throw Structured Exceptions
#pragma warning (disable: 4610)  // class has no user-defined or default constructors

#ifndef _GEOMETRYHELPER_H
#include "DSUtil/Geometry.h"
#endif

#pragma pack(push,8)
namespace Com
{

  /////////////////////////////////////////////////////////////////////////////
  // Smart Pointer helpers


  template <class T1, class T2>
  bool AreComObjectsEqual(T1* p1, T2* p2)
  {
    bool bResult = false;
    if (p1 == NULL && p2 == NULL)
    {
      // Both are NULL
      bResult = true;
    }
    else if (p1 == NULL || p2 == NULL)
    {
      // One is NULL and one is not
      bResult = false;
    }
    else
    {
      // Both are not NULL. Compare IUnknowns.
      IUnknown* pUnk1 = NULL;
      IUnknown* pUnk2 = NULL;
      if (SUCCEEDED(p1->QueryInterface(IID_IUnknown, (void**)&pUnk1)))
      {
        if (SUCCEEDED(p2->QueryInterface(IID_IUnknown, (void**)&pUnk2)))
        {
          bResult = (pUnk1 == pUnk2);
          pUnk2->Release();
        }
        pUnk1->Release();
      }
    }
    return bResult;
  }

  inline IUnknown* SComPtrAssign(IUnknown** pp, IUnknown* lp)
  {
    if (pp == NULL)
      return NULL;

    if (lp != NULL)
      lp->AddRef();
    if (*pp)
      (*pp)->Release();
    *pp = lp;
    return lp;
  }

  inline IUnknown* SComQIPtrAssign(IUnknown** pp, IUnknown* lp, REFIID riid)
  {
    if (pp == NULL)
      return NULL;

    IUnknown* pTemp = *pp;
    *pp = NULL;
    if (lp != NULL)
      lp->QueryInterface(riid, (void**)pp);
    if (pTemp)
      pTemp->Release();
    return *pp;
  }


  /////////////////////////////////////////////////////////////////////////////
  // COM Smart pointers

  template <class T>
  class _SNoAddRefReleaseOnCComPtr : public T
  {
  private:
    STDMETHOD_(ULONG, AddRef)() = 0;
    STDMETHOD_(ULONG, Release)() = 0;
  };

  //SComPtrBase provides the basis for all other smart pointers
  //The other smartpointers add their own constructors and operators
  template <class T>
  class SComPtrBase
  {
  protected:
    SComPtrBase() throw()
    {
      p = NULL;
    }
    SComPtrBase(int nNull) throw()
    {
      SASSERT(nNull == 0);
      (void)nNull;
      p = NULL;
    }
    SComPtrBase(T* lp) throw()
    {
      p = lp;
      if (p != NULL)
        p->AddRef();
    }
  public:
    typedef T _PtrClass;
    ~SComPtrBase() throw()
    {
      if (p)
        p->Release();
    }
    operator T* () const throw()
    {
      return p;
    }
    T& operator*() const
    {
      SASSERT(p != NULL);
      return *p;
    }
    //The assert on operator& usually indicates a bug.  If this is really
    //what is needed, however, take the address of the p member explicitly.
    T** operator&() throw()
    {
      SASSERT(p == NULL);
      return &p;
    }
    _SNoAddRefReleaseOnCComPtr<T>* operator->() const throw()
    {
      SASSERT(p != NULL);
      return (_SNoAddRefReleaseOnCComPtr<T>*)p;
    }
    bool operator!() const throw()
    {
      return (p == NULL);
    }
    bool operator<(T* pT) const throw()
    {
      return p < pT;
    }
    bool operator!=(T* pT) const
    {
      return !operator==(pT);
    }
    bool operator==(T* pT) const throw()
    {
      return p == pT;
    }

    // safe Release() method
    ULONG Release()
    {
      T* ptr = p;
      ULONG result = 0;
      if (ptr)
      {
        p = NULL;
        result = ptr->Release();
      }
      return result;
    }
    // Release the interface even if there's still some references on it
    void FullRelease()
    {
      T* ptr = p;
      if (ptr)
      {
        p = NULL;
        int counter = ptr->Release();
        while (counter != 0)
        {
          counter = ptr->Release();
        }
      }
    }

    // Compare two objects for equivalence
    inline bool IsEqualObject(IUnknown* pOther) throw();
    
    // Attach to an existing interface (does not AddRef)
    void Attach(T* p2) throw()
    {
      if (p)
        p->Release();
      p = p2;
    }
    // Detach the interface (does not Release)
    T* Detach() throw()
    {
      T* pt = p;
      p = NULL;
      return pt;
    }
    HRESULT CopyTo(T** ppT) throw()
    {
      SASSERT(ppT != NULL);
      if (ppT == NULL)
        return E_POINTER;
      *ppT = p;
      if (p)
        p->AddRef();
      return S_OK;
    }
    HRESULT CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter = NULL, DWORD dwClsContext = CLSCTX_ALL) throw()
    {
      SASSERT(p == NULL);
      return ::CoCreateInstance(rclsid, pUnkOuter, dwClsContext, __uuidof(T), (void**)&p);
    }
    HRESULT CoCreateInstance(LPCOLESTR szProgID, LPUNKNOWN pUnkOuter = NULL, DWORD dwClsContext = CLSCTX_ALL) throw()
    {
      CLSID clsid;
      HRESULT hr = CLSIDFromProgID(szProgID, &clsid);
      SASSERT(p == NULL);
      if (SUCCEEDED(hr))
        hr = ::CoCreateInstance(clsid, pUnkOuter, dwClsContext, __uuidof(T), (void**)&p);
      return hr;
    }
    template <class Q>
    HRESULT QueryInterface(Q** pp) const throw()
    {
      SASSERT(pp != NULL);
      return p->QueryInterface(__uuidof(Q), (void**)pp);
    }
    T* p;
  };

  template <class T>
  class SComPtr : public SComPtrBase<T>
  {
  public:
    SComPtr() throw()
    {
    }
    SComPtr(int nNull) throw() :
      SComPtrBase<T>(nNull)
    {
    }
    SComPtr(T* lp) throw() :
      SComPtrBase<T>(lp)

    {
    }
    SComPtr(const SComPtr<T>& lp) throw() :
      SComPtrBase<T>(lp.p)
    {
    }
    T* operator=(T* lp) throw()
    {
      if (*this != lp)
      {
        return static_cast<T*>(SComPtrAssign((IUnknown**)&this->p, lp));
      }
      return *this;
    }
    template <typename Q>
    T* operator=(const SComPtr<Q>& lp) throw()
    {
      if (!IsEqualObject(lp))
      {
        return static_cast<T*>(SComQIPtrAssign((IUnknown**)&this->p, lp, __uuidof(T)));
      }
      return *this;
    }
    T* operator=(const SComPtr<T>& lp) throw()
    {
      if (*this != lp)
      {
        return static_cast<T*>(SComPtrAssign((IUnknown**)&this->p, lp));
      }
      return *this;
    }
  };

  template <class T>
  class SComPtrForList : public SComPtr<T>
  {
  public:
    SComPtrForList() throw()
    {
    }
    SComPtrForList(T* lp) throw() :
      SComPtr<T>(lp)

    {
    }
    SComPtrForList(const SComPtr<T>& lp) throw() :
      SComPtr<T>(lp)
    {
    }
    T* operator=(T* lp) throw()
    {
      if (*this != lp)
      {
        return static_cast<T*>(SComPtrAssign((IUnknown**)&this->m_ptr, lp));
      }
      return *this;
    }
    template <typename Q>
    T* operator=(const SComPtr<Q>& lp) throw()
    {
      if (!AreComObjectsEqual(*this, lp))
      {
        return static_cast<T*>(SComQIPtrAssign((IUnknown**)&this->m_ptr, lp.m_ptr, __uuidof(T)));
      }
      return *this;
    }
    T* operator=(const SComPtr<T>& lp) throw()
    {
      if (!AreComObjectsEqual(this->m_ptr, lp.m_ptr))
      {
        return static_cast<T*>(SComPtrAssign((IUnknown**)&this->m_ptr, lp.m_ptr));
      }
      return *this;
    }

    SComPtrForList* operator&()
    {
      return this;
    }
  };


template <class T>
  inline bool SComPtrBase<T>::IsEqualObject(_Inout_opt_ IUnknown* pOther) throw()
  {
    if (p == NULL && pOther == NULL)
      return true;	// They are both NULL objects

    if (p == NULL || pOther == NULL)
      return false;	// One is NULL the other is not

    SComPtr<IUnknown> punk1;
    SComPtr<IUnknown> punk2;
    p->QueryInterface(__uuidof(IUnknown), (void**)&punk1);
    pOther->QueryInterface(__uuidof(IUnknown), (void**)&punk2);
    return punk1 == punk2;
  }

  template <class T, const IID* piid = &__uuidof(T)>
  class SComQIPtr : public SComPtr<T>
  {
  public:
    SComQIPtr() throw()
    {
    }
    SComQIPtr(T* lp) throw() :
      SComPtr<T>(lp)
    {
    }
    SComQIPtr(const SComQIPtr<T, piid>& lp) throw() :
      SComPtr<T>(lp.p)
    {
    }

    SComQIPtr(IUnknown* lp) throw()
    {
      if (lp != NULL)
        lp->QueryInterface(*piid, (void**)&this->p);
    }

    T* operator=(T* lp) throw()
    {
      if (*this != lp)
      {
        return static_cast<T*>(SComPtrAssign((IUnknown**)&this->p, lp));
      }
      return *this;
    }
    T* operator=(const SComQIPtr<T, piid>& lp) throw()
    {
      if (*this != lp)
      {
        return static_cast<T*>(SComPtrAssign((IUnknown**)&this->p, lp.p));
      }
      return *this;
    }
    T* operator=(IUnknown* lp) throw()
    {
      if (*this != lp)
      {
        return static_cast<T*>(SComQIPtrAssign((IUnknown**)&this->p, lp, *piid));
      }
      return *this;
    }

  };

  //Specialization to make it work
  template<>
  class SComQIPtr<IUnknown, &IID_IUnknown> : public SComPtr<IUnknown>
  {
  public:
    SComQIPtr() throw()
    {
    }
    SComQIPtr(IUnknown* lp) throw()
    {
      //Actually do a QI to get identity
      if (lp != NULL)
        lp->QueryInterface(__uuidof(IUnknown), (void**)&p);
    }
    SComQIPtr(const SComQIPtr<IUnknown, &IID_IUnknown>& lp) throw() :
      SComPtr<IUnknown>(lp.p)
    {
    }

    IUnknown* operator=(IUnknown* lp) throw()
    {
      if (*this != lp)
      {
        //Actually do a QI to get identity
        return SComQIPtrAssign((IUnknown**)&p, lp, __uuidof(IUnknown));
      }
      return *this;
    }

    IUnknown* operator=(const SComQIPtr<IUnknown, &IID_IUnknown>& lp) throw()
    {
        if(*this!=lp)
        {
            return SComPtrAssign((IUnknown**)&p, lp.p);
        }
        return *this;
    }
  };

  typedef SComQIPtr<IDispatch, &__uuidof(IDispatch)> CComDispatchDriver;

}    // namespace Com
#pragma pack(pop)

#pragma warning (pop)    


#endif    // __SCOMCLI_H__
