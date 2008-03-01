/*
 *  cSymbolTable.cc
 *  Avida
 *
 *  Created by David on 2/2/06.
 *  Copyright 1999-2008 Michigan State University. All rights reserved.
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; version 2
 *  of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "cSymbolTable.h"


cSymbolTable::~cSymbolTable()
{
  for (int i = 0; i < m_sym_tbl.GetSize(); i++) delete m_sym_tbl[i];
  for (int i = 0; i < m_fun_tbl.GetSize(); i++) delete m_fun_tbl[i];
}

bool cSymbolTable::AddVariable(const cString& name, ASType_t type, int& var_id)
{
  if (LookupVariable(name, var_id)) return false;

  var_id = m_sym_tbl.GetSize();
  m_sym_tbl.Push(new sSymbolEntry(name, type, m_scope));
  m_sym_dict.Add(name, var_id);
  
  return true;
}

bool cSymbolTable::AddFunction(const cString& name, ASType_t type, int& fun_id)
{
  if (LookupFunction(name, fun_id)) return false;
  
  fun_id = m_fun_tbl.GetSize();
  m_fun_tbl.Push(new sFunctionEntry(name, type, m_scope));
  m_fun_dict.Add(name, fun_id);
  
  return true;
}

void cSymbolTable::PopScope()
{
  m_deactivate_cycle++;
  
  for (int i = 0; i < m_sym_tbl.GetSize(); i++) {
    sSymbolEntry* se = m_sym_tbl[i];
    if (se->scope == m_scope && !se->deactivate) {
      m_sym_dict.Remove(se->name);
      se->deactivate = m_deactivate_cycle;
    }
  }
  
  for (int i = 0; i < m_fun_tbl.GetSize(); i++) {
    sFunctionEntry* fe = m_fun_tbl[i];
    if (fe->scope == m_scope && !fe->deactivate) {
      m_fun_dict.Remove(fe->name);
      fe->deactivate = m_deactivate_cycle;
    }
  }

  m_return = false;
  m_scope--;
}

