#define COBJMACROS
#include "HostAutostart.h"

#include <oleauto.h>
#include <sddl.h>
#include <stdint.h>
#include <stdlib.h>
#include <taskschd.h>
#include <wchar.h>
#include <wctype.h>

/* Some MinGW uuid libraries omit the Task Scheduler 2 interfaces. */
static const GUID scheduler_class =
    {0x0f87369f, 0xa4e5, 0x4cfc, {0xbd, 0x3e, 0x73, 0xe6, 0x15, 0x45, 0x72, 0xdd}};
static const GUID scheduler_interface =
    {0x2faba4c7, 0x4da9, 0x4013, {0x96, 0x97, 0x20, 0xcc, 0x3f, 0xd4, 0x0f, 0x85}};
static const GUID exec_interface =
    {0x4c3d624d, 0xfd6b, 0x49a3, {0xb9, 0xb7, 0x09, 0xcb, 0x3c, 0xd3, 0xf0, 0x47}};
static const GUID logon_interface =
    {0x72dade38, 0xfae4, 0x4b3e, {0xba, 0xf4, 0x5d, 0x00, 0x9a, 0xf0, 0x2b, 0x1c}};

typedef struct SchedulerConnection {
    ITaskService *service;
    ITaskFolder *folder;
    bool uninitialize;
} SchedulerConnection;

static bool valid_arguments(const wchar_t *task_name, const wchar_t *path) {
    if (!task_name || !*task_name || wcschr(task_name, L'\\') ||
            wcschr(task_name, L'/') || !path || !*path) return false;
    return (wcslen(path) >= 3 && iswalpha(path[0]) && path[1] == L':' &&
            (path[2] == L'\\' || path[2] == L'/')) ||
            (path[0] == L'\\' && path[1] == L'\\');
}

static bool missing_task(HRESULT result) {
    return result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
            result == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
}

static void close_scheduler(SchedulerConnection *connection) {
    if (connection->folder) ITaskFolder_Release(connection->folder);
    if (connection->service) ITaskService_Release(connection->service);
    if (connection->uninitialize) CoUninitialize();
}

static HRESULT open_scheduler(SchedulerConnection *connection) {
    VARIANT empty;
    BSTR root;
    HRESULT result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(result)) connection->uninitialize = true;
    else if (result != RPC_E_CHANGED_MODE) return result;
    result = CoCreateInstance(&scheduler_class, NULL, CLSCTX_INPROC_SERVER,
            &scheduler_interface, (void **)&connection->service);
    if (FAILED(result)) return result;
    VariantInit(&empty);
    result = ITaskService_Connect(connection->service, empty, empty, empty, empty);
    if (FAILED(result)) return result;
    root = SysAllocString(L"\\");
    if (!root) return E_OUTOFMEMORY;
    result = ITaskService_GetFolder(connection->service, root, &connection->folder);
    SysFreeString(root);
    return result;
}

static HRESULT current_user_sid(wchar_t **sid_string) {
    HANDLE token = NULL;
    TOKEN_USER *user = NULL;
    DWORD bytes = 0;
    HRESULT result;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return HRESULT_FROM_WIN32(GetLastError());
    if (!GetTokenInformation(token, TokenUser, NULL, 0, &bytes) &&
            GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        result = HRESULT_FROM_WIN32(GetLastError());
        goto cleanup;
    }
    user = malloc(bytes);
    if (!user) {
        result = E_OUTOFMEMORY;
        goto cleanup;
    }
    if (!GetTokenInformation(token, TokenUser, user, bytes, &bytes) ||
            !ConvertSidToStringSidW(user->User.Sid, sid_string)) {
        result = HRESULT_FROM_WIN32(GetLastError());
        goto cleanup;
    }
    result = S_OK;
cleanup:
    free(user);
    CloseHandle(token);
    return result;
}

static HRESULT principal_matches(const wchar_t *user_id, const wchar_t *sid,
        bool *matches) {
    BYTE sid_buffer[SECURITY_MAX_SID_SIZE];
    wchar_t domain[256];
    DWORD sid_bytes = sizeof(sid_buffer), domain_chars = 256;
    SID_NAME_USE use;
    wchar_t *resolved_sid = NULL;
    HRESULT result = S_OK;
    *matches = false;
    if (!user_id || !*user_id) return S_OK;
    if (_wcsicmp(user_id, sid) == 0) {
        *matches = true;
        return S_OK;
    }
    /* Scheduler may normalize the SID supplied at registration to DOMAIN\user. */
    if (!LookupAccountNameW(NULL, user_id, sid_buffer, &sid_bytes, domain,
            &domain_chars, &use)) {
        DWORD error = GetLastError();
        return error == ERROR_NONE_MAPPED ? S_OK : HRESULT_FROM_WIN32(error);
    }
    if (!ConvertSidToStringSidW(sid_buffer, &resolved_sid))
        return HRESULT_FROM_WIN32(GetLastError());
    *matches = _wcsicmp(resolved_sid, sid) == 0;
    LocalFree(resolved_sid);
    return result;
}

static bool only_whitespace(const wchar_t *text) {
    if (!text) return true;
    while (*text && iswspace(*text)) ++text;
    return *text == L'\0';
}

static HRESULT has_user_logon_trigger(ITaskDefinition *definition,
        const wchar_t *sid, bool *matches) {
    ITriggerCollection *triggers = NULL;
    ITrigger *trigger = NULL;
    ILogonTrigger *logon = NULL;
    BSTR user_id = NULL;
    LONG count = 0;
    HRESULT result = ITaskDefinition_get_Triggers(definition, &triggers);
    *matches = false;
    if (FAILED(result)) goto cleanup;
    result = ITriggerCollection_get_Count(triggers, &count);
    if (FAILED(result)) goto cleanup;
    for (LONG index = 1; index <= count; ++index) {
        VARIANT_BOOL trigger_enabled;
        TASK_TRIGGER_TYPE2 type;
        result = ITriggerCollection_get_Item(triggers, index, &trigger);
        if (FAILED(result)) goto cleanup;
        result = ITrigger_get_Enabled(trigger, &trigger_enabled);
        if (FAILED(result)) goto cleanup;
        result = ITrigger_get_Type(trigger, &type);
        if (FAILED(result)) goto cleanup;
        if (trigger_enabled != VARIANT_FALSE && type == TASK_TRIGGER_LOGON) {
            result = ITrigger_QueryInterface(trigger, &logon_interface, (void **)&logon);
            if (FAILED(result)) goto cleanup;
            result = ILogonTrigger_get_UserId(logon, &user_id);
            if (FAILED(result)) goto cleanup;
            if (!user_id || !*user_id) *matches = true;
            else result = principal_matches(user_id, sid, matches);
            if (FAILED(result) || *matches) goto cleanup;
            SysFreeString(user_id);
            user_id = NULL;
            ILogonTrigger_Release(logon);
            logon = NULL;
        }
        ITrigger_Release(trigger);
        trigger = NULL;
    }
cleanup:
    SysFreeString(user_id);
    if (logon) ILogonTrigger_Release(logon);
    if (trigger) ITrigger_Release(trigger);
    if (triggers) ITriggerCollection_Release(triggers);
    return result;
}

HRESULT wb_host_autostart_query(const wchar_t *task_name,
        const wchar_t *executable_path, bool *enabled) {
    SchedulerConnection connection = {0};
    IRegisteredTask *task = NULL;
    ITaskDefinition *definition = NULL;
    IPrincipal *principal = NULL;
    IActionCollection *actions = NULL;
    IAction *action = NULL;
    IExecAction *exec = NULL;
    BSTR name = NULL, user_id = NULL, action_path = NULL, arguments = NULL;
    wchar_t *sid = NULL;
    VARIANT_BOOL task_enabled = VARIANT_FALSE;
    TASK_LOGON_TYPE logon;
    TASK_RUNLEVEL_TYPE run_level;
    LONG count = 0;
    bool matches = false;
    HRESULT result;
    if (enabled) *enabled = false;
    if (!enabled || !valid_arguments(task_name, executable_path)) return E_INVALIDARG;
    result = open_scheduler(&connection);
    if (FAILED(result)) goto cleanup;
    name = SysAllocString(task_name);
    if (!name) { result = E_OUTOFMEMORY; goto cleanup; }
    result = ITaskFolder_GetTask(connection.folder, name, &task);
    if (missing_task(result)) { result = S_OK; goto cleanup; }
    if (FAILED(result)) goto cleanup;
    result = IRegisteredTask_get_Enabled(task, &task_enabled);
    if (FAILED(result) || task_enabled == VARIANT_FALSE) goto cleanup;
    result = IRegisteredTask_get_Definition(task, &definition);
    if (FAILED(result)) goto cleanup;
    result = ITaskDefinition_get_Principal(definition, &principal);
    if (FAILED(result)) goto cleanup;
    result = IPrincipal_get_LogonType(principal, &logon);
    if (FAILED(result) || logon != TASK_LOGON_INTERACTIVE_TOKEN) goto cleanup;
    result = IPrincipal_get_RunLevel(principal, &run_level);
    if (FAILED(result) || run_level != TASK_RUNLEVEL_LUA) goto cleanup;
    result = IPrincipal_get_UserId(principal, &user_id);
    if (FAILED(result)) goto cleanup;
    result = current_user_sid(&sid);
    if (FAILED(result)) goto cleanup;
    result = principal_matches(user_id, sid, &matches);
    if (FAILED(result) || !matches) goto cleanup;
    result = ITaskDefinition_get_Actions(definition, &actions);
    if (FAILED(result)) goto cleanup;
    result = IActionCollection_get_Count(actions, &count);
    if (FAILED(result) || count != 1) goto cleanup;
    result = IActionCollection_get_Item(actions, 1, &action);
    if (FAILED(result)) goto cleanup;
    result = IAction_QueryInterface(action, &exec_interface, (void **)&exec);
    if (result == E_NOINTERFACE) { result = S_OK; goto cleanup; }
    if (FAILED(result)) goto cleanup;
    result = IExecAction_get_Path(exec, &action_path);
    if (FAILED(result) || !action_path || _wcsicmp(action_path, executable_path) != 0)
        goto cleanup;
    result = IExecAction_get_Arguments(exec, &arguments);
    if (FAILED(result) || !only_whitespace(arguments)) goto cleanup;
    result = has_user_logon_trigger(definition, sid, enabled);
cleanup:
    LocalFree(sid);
    SysFreeString(arguments);
    SysFreeString(action_path);
    SysFreeString(user_id);
    SysFreeString(name);
    if (exec) IExecAction_Release(exec);
    if (action) IAction_Release(action);
    if (actions) IActionCollection_Release(actions);
    if (principal) IPrincipal_Release(principal);
    if (definition) ITaskDefinition_Release(definition);
    if (task) IRegisteredTask_Release(task);
    close_scheduler(&connection);
    return result;
}

static wchar_t *escape_xml(const wchar_t *text) {
    size_t length = wcslen(text), offset = 0;
    wchar_t *escaped;
    if (length > (SIZE_MAX / sizeof(wchar_t) - 1) / 6) return NULL;
    escaped = malloc((length * 6 + 1) * sizeof(wchar_t));
    if (!escaped) return NULL;
    for (size_t i = 0; i < length; ++i) {
        const wchar_t *entity = NULL;
        switch (text[i]) {
            case L'&': entity = L"&amp;"; break;
            case L'<': entity = L"&lt;"; break;
            case L'>': entity = L"&gt;"; break;
            case L'\"': entity = L"&quot;"; break;
            case L'\'': entity = L"&apos;"; break;
            default: escaped[offset++] = text[i]; break;
        }
        if (entity) {
            size_t entity_length = wcslen(entity);
            memcpy(escaped + offset, entity, entity_length * sizeof(wchar_t));
            offset += entity_length;
        }
    }
    escaped[offset] = L'\0';
    return escaped;
}

static HRESULT register_task(SchedulerConnection *connection, BSTR name,
        const wchar_t *path) {
    static const wchar_t format[] =
        L"<?xml version=\"1.0\" encoding=\"UTF-16\"?>"
        L"<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">"
        L"<Triggers><LogonTrigger><Enabled>true</Enabled><UserId>%ls</UserId>"
        L"<Delay>PT15S</Delay></LogonTrigger></Triggers>"
        L"<Principals><Principal id=\"CurrentUser\"><UserId>%ls</UserId>"
        L"<LogonType>InteractiveToken</LogonType><RunLevel>LeastPrivilege</RunLevel>"
        L"</Principal></Principals><Settings>"
        L"<MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>"
        L"<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>"
        L"<StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>"
        L"<StartWhenAvailable>true</StartWhenAvailable>"
        L"<Enabled>true</Enabled><ExecutionTimeLimit>PT0S</ExecutionTimeLimit>"
        L"<RestartOnFailure><Interval>PT1M</Interval><Count>3</Count></RestartOnFailure>"
        L"</Settings><Actions Context=\"CurrentUser\"><Exec><Command>%ls</Command>"
        L"<WorkingDirectory>%ls</WorkingDirectory>"
        L"</Exec></Actions></Task>";
    wchar_t *sid = NULL, *escaped = NULL, *directory = NULL;
    wchar_t *escaped_directory = NULL, *xml = NULL;
    BSTR xml_bstr = NULL;
    IRegisteredTask *task = NULL;
    VARIANT empty, user;
    size_t chars;
    HRESULT result = current_user_sid(&sid);
    VariantInit(&empty);
    VariantInit(&user);
    if (FAILED(result)) goto cleanup;
    escaped = escape_xml(path);
    if (!escaped) { result = E_OUTOFMEMORY; goto cleanup; }
    directory = malloc((wcslen(path) + 1) * sizeof(wchar_t));
    if (!directory) { result = E_OUTOFMEMORY; goto cleanup; }
    wcscpy(directory, path);
    wchar_t *separator = wcsrchr(directory, L'\\');
    wchar_t *forward_separator = wcsrchr(directory, L'/');
    if (!separator || (forward_separator && forward_separator > separator))
        separator = forward_separator;
    if (!separator) { result = E_INVALIDARG; goto cleanup; }
    if (separator == directory + 2 && directory[1] == L':') ++separator;
    *separator = L'\0';
    escaped_directory = escape_xml(directory);
    if (!escaped_directory) { result = E_OUTOFMEMORY; goto cleanup; }
    chars = wcslen(format) + 2 * wcslen(sid) + wcslen(escaped) +
            wcslen(escaped_directory) + 1;
    xml = malloc(chars * sizeof(wchar_t));
    if (!xml) { result = E_OUTOFMEMORY; goto cleanup; }
    if (swprintf(xml, chars, format, sid, sid, escaped, escaped_directory) < 0) {
        result = E_FAIL;
        goto cleanup;
    }
    xml_bstr = SysAllocString(xml);
    user.vt = VT_BSTR;
    user.bstrVal = SysAllocString(sid);
    if (!xml_bstr || !user.bstrVal) { result = E_OUTOFMEMORY; goto cleanup; }
    result = ITaskFolder_RegisterTask(connection->folder, name, xml_bstr,
            TASK_CREATE_OR_UPDATE, user, empty, TASK_LOGON_INTERACTIVE_TOKEN,
            empty, &task);
cleanup:
    if (task) IRegisteredTask_Release(task);
    VariantClear(&user);
    SysFreeString(xml_bstr);
    free(xml);
    free(escaped_directory);
    free(directory);
    free(escaped);
    LocalFree(sid);
    return result;
}

/* Remove only the old, direct launch of this same product executable. */
static HRESULT migrate_legacy_run(const wchar_t *task_name, const wchar_t *path) {
    static const wchar_t run_key[] =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const wchar_t *basename = wcsrchr(path, L'\\');
    wchar_t *value = NULL, *target, *end;
    DWORD bytes = 0;
    LSTATUS status;
    HKEY key = NULL;
    if (wcscmp(task_name, WB_AUTOSTART_TASK_NAME) != 0 || !basename ||
            _wcsicmp(basename + 1, L"WorkBuddyAltTabHost.exe") != 0) return S_OK;
    status = RegGetValueW(HKEY_CURRENT_USER, run_key, task_name,
            RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, NULL, NULL, &bytes);
    if (status == ERROR_FILE_NOT_FOUND) return S_OK;
    if (status != ERROR_SUCCESS) return HRESULT_FROM_WIN32(status);
    value = calloc((size_t)bytes + sizeof(wchar_t), 1);
    if (!value) return E_OUTOFMEMORY;
    status = RegGetValueW(HKEY_CURRENT_USER, run_key, task_name,
            RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, NULL, value, &bytes);
    if (status != ERROR_SUCCESS) goto cleanup;
    target = value;
    while (iswspace(*target)) ++target;
    end = target + wcslen(target);
    while (end > target && iswspace(end[-1])) --end;
    *end = L'\0';
    if (*target == L'\"' && end > target + 1 && end[-1] == L'\"') {
        ++target;
        end[-1] = L'\0';
    }
    if (_wcsicmp(target, path) != 0) goto cleanup;
    status = RegOpenKeyExW(HKEY_CURRENT_USER, run_key, 0, KEY_SET_VALUE, &key);
    if (status == ERROR_SUCCESS) status = RegDeleteValueW(key, task_name);
cleanup:
    if (key) RegCloseKey(key);
    free(value);
    return status == ERROR_FILE_NOT_FOUND ? S_OK : HRESULT_FROM_WIN32(status);
}

HRESULT wb_host_autostart_set(const wchar_t *task_name,
        const wchar_t *executable_path, bool enabled) {
    SchedulerConnection connection = {0};
    BSTR name = NULL;
    HRESULT result;
    if (!valid_arguments(task_name, executable_path)) return E_INVALIDARG;
    result = open_scheduler(&connection);
    if (FAILED(result)) goto cleanup;
    name = SysAllocString(task_name);
    if (!name) { result = E_OUTOFMEMORY; goto cleanup; }
    if (enabled) result = register_task(&connection, name, executable_path);
    else {
        result = ITaskFolder_DeleteTask(connection.folder, name, 0);
        if (missing_task(result)) result = S_OK;
    }
    if (SUCCEEDED(result)) result = migrate_legacy_run(task_name, executable_path);
cleanup:
    SysFreeString(name);
    close_scheduler(&connection);
    return result;
}
