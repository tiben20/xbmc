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

#include "stdafx.h"
#include "NodeFactory.h"
#include "Exception.h"

namespace ssf
{
  NodeFactory::NodeFactory()
    : m_counter(0)
    , m_root(NULL)
    , m_predefined(false)
  {
  }

  NodeFactory::~NodeFactory()
  {
    clear();
  }

  std::wstring NodeFactory::GenName()
  {
    std::wstring name;
    name.Format(L"%I64d", m_counter++);
    return name;
  }

  void NodeFactory::clear()
  {
    m_root = NULL;

    StringMapW<Node *>::iterator it = m_nodes.begin();
    for (; it != m_nodes.end(); ++it)
      delete it->second;

    m_nodes.clear();
    m_newnodes.clear();
  }

  void NodeFactory::Commit()
  {
    m_newnodes.clear();
  }

  void NodeFactory::Rollback()
  {
    std::list<std::wstring>::reverse_iterator it = m_newnodes.rbegin();
    for(; it != m_newnodes.rend(); ++it)
    {
      StringMapW<Node *>::iterator it2 = m_nodes.find(*it);
      if(it2 != m_nodes.end())
      {
        delete it2->second; // TODO: remove it from "parent"->m_nodes too
        m_nodes.erase(it2);
      }
    }
  }

  Reference* NodeFactory::CreateRootRef()
  {
    clear();
    m_root = CreateRef(NULL);
    return m_root;
  }

  Reference* NodeFactory::GetRootRef() const
  {
    ASSERT(m_root);
    return m_root;
  }

  Reference* NodeFactory::CreateRef(Definition* pParentDef)
  {
    std::wstring name = GenName();

    Reference* pRef = DNew Reference(this, name);

    m_nodes[name] = pRef;
    m_newnodes.push_back(name);

    if(pParentDef)
    {
      pParentDef->push_back(pRef);
      pRef->m_parent = pParentDef;
    }

    return pRef;
  }

  Definition* NodeFactory::CreateDef(Reference* pParentRef, std::wstring type, std::wstring name, NodePriority priority)
  {
    Definition* pDef = NULL;

    if(name.empty())
    {
      name = GenName();
    }
    else 
    {
      pDef = GetDefByName(name);

      if(pDef)
      {
        if(!pDef->m_predefined)
        {
          throw Exception(_T("redefinition of '%s' is not allowed"), std::string(name));
        }

        if(!pDef->IsTypeUnknown() && !pDef->IsType(type))
        {
          throw Exception(_T("cannot redefine type of %s to %s"), std::string(name), std::string(type));
        }
      }
    }

    if(!pDef)
    {
      pDef = DNew Definition(this, name);

      m_nodes[name] = pDef;
      m_newnodes.push_back(name);

      if(pParentRef)
      {
        pParentRef->push_back(pDef);
        pDef->m_parent = pParentRef;
      }
    }

    pDef->m_type = type;
    pDef->m_priority = priority;
    pDef->m_predefined = m_predefined;

    return pDef;
  }

  Definition* NodeFactory::GetDefByName(std::wstring name) const
  {
    StringMapW<Node *>::const_iterator it = m_nodes.find(name);
    return dynamic_cast<Definition*>(it->second);
  }

  void NodeFactory::GetNewDefs(std::list<Definition*>& defs)
  {
    defs.clear();

    std::list<std::wstring>::iterator it = m_newnodes.begin();
    for(; it != m_newnodes.end(); ++it)
    {
      if(Definition* pDef = GetDefByName(*it))
      {
        defs.push_back(pDef);
      }
    }
  }

  void NodeFactory::Dump(OutputStream& s) const
  {
    if(!m_root) return;

    std::list<Node *>::iterator it = m_root->m_nodes.begin();
    for(; it != m_root->m_nodes.end(); ++it) (*it)->Dump(s);
  }
}
