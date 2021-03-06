#include "stdafx.h"
#include "win32_misc.h"
#include "TypeFind.h"
#include "SmartStrStr.h"

#ifdef FOOBAR2000_MOBILE_WINDOWS
#include <pfc/pp-winapi.h>
#endif

#ifdef _WIN32

mutexScope::mutexScope(HANDLE hMutex_, abort_callback & abort) : hMutex(hMutex_) {
	HANDLE h[2] = { hMutex, abort.get_abort_event() };
	switch (WaitForMultipleObjectsEx(2, h, FALSE, INFINITE, FALSE)) {
	case WAIT_OBJECT_0:
		break; // and enter
	case WAIT_OBJECT_0 + 1:
		throw exception_aborted();
	default:
		uBugCheck();
	}
}
mutexScope::~mutexScope() {
	ReleaseMutex(hMutex);
}

CMutex::CMutex(const TCHAR * name) {
	WIN32_OP_CRITICAL("CreateMutex", m_hMutex = CreateMutex(NULL, FALSE, name));
}
CMutex::~CMutex() {
	CloseHandle(m_hMutex);
}

void CMutex::AcquireByHandle(HANDLE hMutex, abort_callback & aborter) {
	SetLastError(0);
	HANDLE hWait[2] = { hMutex, aborter.get_abort_event() };
	switch (WaitForMultipleObjects(2, hWait, FALSE, INFINITE)) {
	case WAIT_FAILED:
		WIN32_OP_FAIL_CRITICAL("WaitForSingleObject");
	case WAIT_OBJECT_0:
		return;
	case WAIT_OBJECT_0 + 1:
		PFC_ASSERT(aborter.is_aborting());
		throw exception_aborted();
	default:
		uBugCheck();
	}
}

void CMutex::Acquire(abort_callback& aborter) {
	AcquireByHandle(Handle(), aborter);
}

void CMutex::Release() {
	ReleaseMutex(Handle());
}


CMutexScope::CMutexScope(CMutex & mutex, DWORD timeOutMS, const char * timeOutBugMsg) : m_mutex(mutex) {
	SetLastError(0);
	const unsigned div = 4;
	for (unsigned walk = 0; walk < div; ++walk) {
		switch (WaitForSingleObject(m_mutex.Handle(), timeOutMS / div)) {
		case WAIT_FAILED:
			WIN32_OP_FAIL_CRITICAL("WaitForSingleObject");
		case WAIT_OBJECT_0:
			return;
		case WAIT_TIMEOUT:
			break;
		default:
			uBugCheck();
		}
	}
	TRACK_CODE(timeOutBugMsg, uBugCheck());
}

CMutexScope::CMutexScope(CMutex & mutex) : m_mutex(mutex) {
	SetLastError(0);
	switch (WaitForSingleObject(m_mutex.Handle(), INFINITE)) {
	case WAIT_FAILED:
		WIN32_OP_FAIL_CRITICAL("WaitForSingleObject");
	case WAIT_OBJECT_0:
		return;
	default:
		uBugCheck();
	}
}

CMutexScope::CMutexScope(CMutex & mutex, abort_callback & aborter) : m_mutex(mutex) {
	mutex.Acquire(aborter);
}

CMutexScope::~CMutexScope() {
	ReleaseMutex(m_mutex.Handle());
}

#endif

#ifdef FOOBAR2000_DESKTOP_WINDOWS

void registerclass_scope_delayed::toggle_on(UINT p_style,WNDPROC p_wndproc,int p_clsextra,int p_wndextra,HICON p_icon,HCURSOR p_cursor,HBRUSH p_background,const TCHAR * p_class_name,const TCHAR * p_menu_name) {
	toggle_off();
	WNDCLASS wc = {};
	wc.style = p_style;
	wc.lpfnWndProc = p_wndproc;
	wc.cbClsExtra = p_clsextra;
	wc.cbWndExtra = p_wndextra;
	wc.hInstance = core_api::get_my_instance();
	wc.hIcon = p_icon;
	wc.hCursor = p_cursor;
	wc.hbrBackground = p_background;
	wc.lpszMenuName = p_menu_name;
	wc.lpszClassName = p_class_name;
	WIN32_OP_CRITICAL("RegisterClass", (m_class = RegisterClass(&wc)) != 0);
}

void registerclass_scope_delayed::toggle_off() {
	if (m_class != 0) {
		UnregisterClass((LPCTSTR)m_class,core_api::get_my_instance());
		m_class = 0;
	}
}

unsigned QueryScreenDPI(HWND wnd) {
	HDC dc = GetDC(wnd);
	unsigned ret = GetDeviceCaps(dc,LOGPIXELSY);
	ReleaseDC(wnd,dc);
	return ret;
}
unsigned QueryScreenDPI_X(HWND wnd) {
	HDC dc = GetDC(wnd);
	unsigned ret = GetDeviceCaps(dc,LOGPIXELSX);
	ReleaseDC(wnd,dc);
	return ret;
}
unsigned QueryScreenDPI_Y(HWND wnd) {
	HDC dc = GetDC(wnd);
	unsigned ret = GetDeviceCaps(dc,LOGPIXELSY);
	ReleaseDC(wnd,dc);
	return ret;
}

SIZE QueryScreenDPIEx(HWND wnd) {
	HDC dc = GetDC(wnd);
	SIZE ret = { GetDeviceCaps(dc,LOGPIXELSX), GetDeviceCaps(dc,LOGPIXELSY) };
	ReleaseDC(wnd,dc);
	return ret;
}


bool IsMenuNonEmpty(HMENU menu) {
	unsigned n,m=GetMenuItemCount(menu);
	for(n=0;n<m;n++) {
		if (GetSubMenu(menu,n)) return true;
		if (!(GetMenuState(menu,n,MF_BYPOSITION)&MF_SEPARATOR)) return true;
	}
	return false;
}

PFC_NORETURN PFC_NOINLINE void WIN32_OP_FAIL() {
	const DWORD code = GetLastError();
	PFC_ASSERT( code != NO_ERROR );
	throw exception_win32(code);
}

PFC_NORETURN PFC_NOINLINE void WIN32_OP_FAIL_CRITICAL(const char * what) {
	const DWORD code = GetLastError();
	PFC_ASSERT( code != NO_ERROR );
	pfc::string_formatter msg; msg << what << " failure #" << (uint32_t)code;
	TRACK_CODE(msg.get_ptr(), uBugCheck());
}

#ifdef _DEBUG
void WIN32_OP_D_FAIL(const wchar_t * _Message, const wchar_t *_File, unsigned _Line) {
	const DWORD code = GetLastError();
	pfc::array_t<wchar_t> msgFormatted; msgFormatted.set_size(pfc::strlen_t(_Message) + 64);
	wsprintfW(msgFormatted.get_ptr(), L"%s (code: %u)", _Message, code);
	if (IsDebuggerPresent()) {
		OutputDebugString(TEXT("WIN32_OP_D() failure:\n"));
		OutputDebugString(msgFormatted.get_ptr());
		OutputDebugString(TEXT("\n"));
		pfc::crash();
	}
	_wassert(msgFormatted.get_ptr(),_File,_Line);
}
#endif


void GetOSVersionString(pfc::string_base & out) {
	out.reset(); GetOSVersionStringAppend(out);
}

static bool running_under_wine(void) {
    HMODULE module = GetModuleHandle(_T("ntdll.dll"));
    if (!module) return false;
    return GetProcAddress(module, "wine_server_call") != NULL;
}
static bool FetchWineInfoAppend(pfc::string_base & out) {
	typedef const char *(__cdecl *t_wine_get_build_id)(void);
    typedef void (__cdecl *t_wine_get_host_version)( const char **sysname, const char **release );
	const HMODULE ntdll = GetModuleHandle(_T("ntdll.dll"));
	if (ntdll == NULL) return false;
	t_wine_get_build_id wine_get_build_id;
	t_wine_get_host_version wine_get_host_version;
    wine_get_build_id = (t_wine_get_build_id)GetProcAddress(ntdll, "wine_get_build_id");
    wine_get_host_version = (t_wine_get_host_version)GetProcAddress(ntdll, "wine_get_host_version");
	if (wine_get_build_id == NULL || wine_get_host_version == NULL) {
		if (GetProcAddress(ntdll, "wine_server_call") != NULL) {
			out << "wine (unknown version)";
			return true;
		}
		return false;
	}
	const char * sysname = NULL; const char * release = NULL;
	wine_get_host_version(&sysname, &release);
	out << wine_get_build_id() << ", on: " << sysname << " / " << release;
	return true;
}
void GetOSVersionStringAppend(pfc::string_base & out) {

	if (FetchWineInfoAppend(out)) return;

	OSVERSIONINFO ver = {}; ver.dwOSVersionInfoSize = sizeof(ver);
	WIN32_OP( GetVersionEx(&ver) );
	SYSTEM_INFO info = {};
	GetNativeSystemInfo(&info);
	
	out << "Windows " << (int)ver.dwMajorVersion << "." << (int)ver.dwMinorVersion << "." << (int)ver.dwBuildNumber;
	if (ver.szCSDVersion[0] != 0) out << " " << pfc::stringcvt::string_utf8_from_os(ver.szCSDVersion, PFC_TABSIZE(ver.szCSDVersion));
	
	switch(info.wProcessorArchitecture) {
		case PROCESSOR_ARCHITECTURE_AMD64:
			out << " x64"; break;
		case PROCESSOR_ARCHITECTURE_IA64:
			out << " IA64"; break;
		case PROCESSOR_ARCHITECTURE_INTEL:
			out << " x86"; break;
	}
}


void SetDefaultMenuItem(HMENU p_menu,unsigned p_id) {
	MENUITEMINFO info = {sizeof(info)};
	info.fMask = MIIM_STATE;
	GetMenuItemInfo(p_menu,p_id,FALSE,&info);
	info.fState |= MFS_DEFAULT;
	SetMenuItemInfo(p_menu,p_id,FALSE,&info);
}

bool SetClipboardDataBlock(UINT p_format, const void * p_block, t_size p_block_size) {
	bool success = false;
	if (OpenClipboard(NULL)) {
		EmptyClipboard();
		HANDLE handle = GlobalAlloc(GMEM_MOVEABLE, p_block_size);
		if (handle == NULL) {
			CloseClipboard();
			throw std::bad_alloc();
		}
		{CGlobalLock lock(handle);memcpy(lock.GetPtr(), p_block, p_block_size);}
		if (SetClipboardData(p_format, handle) == NULL) {
			GlobalFree(handle);//todo?
		}
		else {
			success = true;
		}
		CloseClipboard();
	}
	return success;
}

void CModelessDialogEntry::Set(HWND p_new) {
	auto api = modeless_dialog_manager::get();
	if (m_wnd) api->remove(m_wnd);
	m_wnd = p_new;
	if (m_wnd) api->add(m_wnd);
}

OleInitializeScope::OleInitializeScope() {
	if (FAILED(OleInitialize(NULL))) throw pfc::exception("OleInitialize() failure");
}
OleInitializeScope::~OleInitializeScope() {
	OleUninitialize();
}

CoInitializeScope::CoInitializeScope() {
	if (FAILED(CoInitialize(NULL))) throw pfc::exception("CoInitialize() failed");
}
CoInitializeScope::CoInitializeScope(DWORD params) {
	if (FAILED(CoInitializeEx(NULL, params))) throw pfc::exception("CoInitialize() failed");
}
CoInitializeScope::~CoInitializeScope() {
	CoUninitialize();
}

void winLocalFileScope::open(const char * inPath, file::ptr inReader, abort_callback & aborter) {
	close();
	if (inPath != NULL) {
		if (_extract_native_path_ptr(inPath)) {
			pfc::string8 prefixed;
			pfc::winPrefixPath(prefixed, inPath);
			m_path = pfc::stringcvt::string_wide_from_utf8(prefixed);
			m_isTemp = false;
			return;
		}
	}

	pfc::string8 tempPath;
	if (!uGetTempPath(tempPath)) uBugCheck();
	tempPath.add_filename(PFC_string_formatter() << pfc::print_guid(pfc::createGUID()) << ".rar");

	m_path = pfc::stringcvt::string_wide_from_utf8(tempPath);

	if (inReader.is_empty()) {
		if (inPath == NULL) uBugCheck();
		inReader = fileOpenReadExisting(inPath, aborter, 1.0);
	}

	file::ptr writer = fileOpenWriteNew(PFC_string_formatter() << "file://" << tempPath, aborter, 1.0);
	file::g_transfer_file(inReader, writer, aborter);
	m_isTemp = true;
}
void winLocalFileScope::close() {
	if (m_isTemp && m_path.length() > 0) {
		pfc::lores_timer timer;
		timer.start();
		for (;;) {
			if (DeleteFile(m_path.c_str())) break;
			if (timer.query() > 1.0) break;
			Sleep(10);
		}
	}
	m_path.clear();
}

LRESULT RelayEraseBkgnd(HWND p_from, HWND p_to, HDC p_dc) {
	LRESULT status;
	POINT pt = { 0, 0 }, pt_old = { 0,0 };
	MapWindowPoints(p_from, p_to, &pt, 1);
	OffsetWindowOrgEx(p_dc, pt.x, pt.y, &pt_old);
	status = SendMessage(p_to, WM_ERASEBKGND, (WPARAM)p_dc, 0);
	SetWindowOrgEx(p_dc, pt_old.x, pt_old.y, 0);
	return status;
}



// TYPEFIND IMPLEMENTATION

namespace TypeFindImpl {

	static size_t _ItemCount(HWND wnd) {
		return ListView_GetItemCount(wnd);
	}
	static const wchar_t * _ItemText(HWND wnd, size_t index, int subItem = 0) {
		NMLVDISPINFO info = {};
		info.hdr.code = LVN_GETDISPINFO;
		info.hdr.idFrom = GetDlgCtrlID(wnd);
		info.hdr.hwndFrom = wnd;
		info.item.iItem = index;
		info.item.iSubItem = subItem;
		info.item.mask = LVIF_TEXT;
		::SendMessage(::GetParent(wnd), WM_NOTIFY, info.hdr.idFrom, reinterpret_cast<LPARAM>(&info));
		if (info.item.pszText == NULL) return L"";
		return info.item.pszText;
	}

};

LRESULT TypeFind::Handler(NMHDR* hdr, int subItemFrom, int subItemCnt) {
	using namespace TypeFindImpl;
	NMLVFINDITEM * info = reinterpret_cast<NMLVFINDITEM*>(hdr);
	const HWND wnd = hdr->hwndFrom;
	if (info->lvfi.flags & LVFI_NEARESTXY) return -1;
	const size_t count = _ItemCount(wnd);
	if (count == 0) return -1;
	const size_t base = (size_t)info->iStart % count;

	static SmartStrStr tool;

	for (size_t walk = 0; walk < count; ++walk) {
		const size_t index = (walk + base) % count;
		for (int subItem = subItemFrom; subItem < subItemFrom + subItemCnt; ++subItem) {
			if (tool.matchHereW(_ItemText(wnd, index, subItem), info->lvfi.psz)) return (LRESULT)index;
		}
	}
	for (size_t walk = 0; walk < count; ++walk) {
		const size_t index = (walk + base) % count;
		for (int subItem = subItemFrom; subItem < subItemFrom + subItemCnt; ++subItem) {
			if ( tool.strStrEndW(_ItemText(wnd, index, subItem), info->lvfi.psz) ) return (LRESULT)index;
		}
	}
	return -1;
}



#endif // FOOBAR2000_DESKTOP_WINDOWS



