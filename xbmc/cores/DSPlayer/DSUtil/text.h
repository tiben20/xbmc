#pragma once

//extern std::wstring ExplodeMin(std::wstring str, std::list<std::wstring>& sl, TCHAR sep, int limit = 0);
//extern std::wstring Explode(std::wstring str, std::list<std::wstring>& sl, TCHAR sep, int limit = 0);
//extern std::wstring Implode(std::list<std::wstring>& sl, TCHAR sep);

template<class T, typename SEP>
T Explode(T str, std::list<T>& sl, SEP sep, int limit = 0)
{
  sl.clear();

  for(int i = 0, j = 0; ; i = j+1)
  {
    j = str.Find(sep, i);

    if(j < 0 || sl.size() == limit-1)
    {
      sl.push_back(str.Mid(i).Trim());
      break;
    }
    else
    {
      sl.push_back(str.Mid(i, j-i).Trim());
    }    
  }

  return sl.front();
}

template<class T, typename SEP>
T ExplodeMin(T str, std::list<T>& sl, SEP sep, int limit = 0)
{
  Explode(str, sl, sep, limit);
  POSITION pos = sl.GetHeadPosition();
  while(pos) 
  {
    POSITION tmp = pos;
    if(sl.GetNext(pos).IsEmpty())
      sl.RemoveAt(tmp);
  }
  if(sl.IsEmpty()) sl.push_back(T()); // eh

  return sl.GetHead();
}
/*
template<class T, typename SEP>
T Implode(std::list<T>& sl, SEP sep)
{
  T ret;
  std::list<T>::iterator it = sl.begin();
  for(; it != sl.end(); ++it)
  {
    ret += *it;
    if (it != sl.end()) ret += sep;
  }
  return(ret);
}
*/
extern std::wstring ExtractTag(std::wstring tag, std::map<std::wstring, std::wstring>& attribs, bool& fClosing);
extern std::string ConvertMBCS(std::string str, DWORD SrcCharSet, DWORD DstCharSet);
extern std::string UrlEncode(std::string str, bool fRaw = false);
extern std::string UrlDecode(std::string str, bool fRaw = false);
extern DWORD CharSetToCodePage(DWORD dwCharSet);
extern std::list<std::wstring>& MakeLower(std::list<std::wstring>& sl);
extern std::list<std::wstring>& MakeUpper(std::list<std::wstring>& sl);
extern std::list<std::wstring>& RemoveStrings(std::list<std::wstring>& sl, int minlen, int maxlen);

