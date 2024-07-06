/*
* Copyright (c) 2023-2024 David Xanatos, xanasoft.com
* All rights reserved.
*
* This file is part of MajorPrivacy.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
*/
#include "pch.h"

#include "../../Library/API/PrivacyAPI.h"
#include "Access/AccessRule.h"
#include "Programs/ProgramID.h"
#include "Programs/ProgramRule.h"
#include "Network/Firewall/FirewallRule.h"
#include "Tweaks/Tweak.h"
#include "Programs/ProgramLibrary.h"
#include "Programs/ProgramItem.h"
#include "Programs/WindowsService.h"
#include "Programs/ProgramPattern.h"
#include "Programs/AppInstallation.h"

#define XVariant CVariant
#define TO_STR(x) (x)
#define AS_STR(x) (x).AsStr()
#define AS_LIST(x) (x).AsList<std::wstring>()
#define AS_ALIST(x) (x).AsList<std::string>()
#define IS_EMPTY(x) (x).empty()
#define GET_PATH TO_STR
#define SET_PATH(x, y) (x) = (y)
#define ASTR std::string
#define CFwRule CFirewallRule

#include "../../Library/API/API_GenericRule.cpp"

#include "../../Library/API/API_ProgramRule.cpp"

#include "../../Library/API/API_AccessRule.cpp"

#include "Network/Firewall/WindowsFirewall.h"
#include "../Library/Common/Strings.h"
#include "../../Library/API/API_FwRule.cpp"

#include "../../Library/API/API_ProgramID.cpp"

#define PROG_SVC
#include "../../Library/API/API_Programs.cpp"

#define TWEAKS_WO
#include "../../Library/API/API_Tweak.cpp"