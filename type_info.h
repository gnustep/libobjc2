#ifndef _TYPE_INFO_H_INCLUDED_
#define _TYPE_INFO_H_INCLUDED_

#if CXX_ABI_IS_GNU == 1
#	include "type_info_gnu.h"
#else
#	include "type_info_llvm.h"
#endif
#endif //_TYPE_INFO_H_INCLUDED_