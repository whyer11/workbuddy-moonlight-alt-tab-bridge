#define COBJMACROS
#include <windows.h>
#include <oleauto.h>
#include <stdio.h>
#include <string.h>
#include <taskschd.h>
#include <wchar.h>
#include "../src/HostAutostart.h"

static int require_success(HRESULT result, const char *operation) {
    if (FAILED(result)) {
        fprintf(stderr, "%s failed: 0x%08lx\n", operation, (unsigned long)result);
        return 0;
    }
    return 1;
}

static int verify_registered_configuration_and_disable(const wchar_t *task_name) {
    static const GUID scheduler_class =
        {0x0f87369f, 0xa4e5, 0x4cfc, {0xbd, 0x3e, 0x73, 0xe6, 0x15, 0x45, 0x72, 0xdd}};
    static const GUID scheduler_interface =
        {0x2faba4c7, 0x4da9, 0x4013, {0x96, 0x97, 0x20, 0xcc, 0x3f, 0xd4, 0x0f, 0x85}};
    static const GUID logon_interface =
        {0x72dade38, 0xfae4, 0x4b3e, {0xba, 0xf4, 0x5d, 0x00, 0x9a, 0xf0, 0x2b, 0x1c}};
    ITaskService *service = NULL;
    ITaskFolder *folder = NULL;
    IRegisteredTask *task = NULL;
    ITaskDefinition *definition = NULL;
    ITaskSettings *settings = NULL;
    ITriggerCollection *triggers = NULL;
    ITrigger *trigger = NULL;
    ILogonTrigger *logon = NULL;
    VARIANT empty;
    BSTR root = NULL, name = NULL, interval = NULL, limit = NULL, delay = NULL;
    VARIANT_BOOL battery_disallow, battery_stop;
    TASK_INSTANCES_POLICY instances;
    INT restart_count = 0;
    LONG trigger_count = 0;
    int result = 0;
    HRESULT initialized = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (!require_success(initialized, "test COM initialization")) return 0;
    VariantInit(&empty);
    root = SysAllocString(L"\\");
    name = SysAllocString(task_name);
    if (!root || !name) goto cleanup;
#define CHECK_COM(operation) do { if (!require_success((operation), #operation)) goto cleanup; } while (0)
    CHECK_COM(CoCreateInstance(&scheduler_class, NULL, CLSCTX_INPROC_SERVER,
            &scheduler_interface, (void **)&service));
    CHECK_COM(ITaskService_Connect(service, empty, empty, empty, empty));
    CHECK_COM(ITaskService_GetFolder(service, root, &folder));
    CHECK_COM(ITaskFolder_GetTask(folder, name, &task));
    CHECK_COM(IRegisteredTask_get_Definition(task, &definition));
    CHECK_COM(ITaskDefinition_get_Settings(definition, &settings));
    CHECK_COM(ITaskSettings_get_RestartCount(settings, &restart_count));
    CHECK_COM(ITaskSettings_get_RestartInterval(settings, &interval));
    CHECK_COM(ITaskSettings_get_ExecutionTimeLimit(settings, &limit));
    CHECK_COM(ITaskSettings_get_DisallowStartIfOnBatteries(settings, &battery_disallow));
    CHECK_COM(ITaskSettings_get_StopIfGoingOnBatteries(settings, &battery_stop));
    CHECK_COM(ITaskSettings_get_MultipleInstances(settings, &instances));
    CHECK_COM(ITaskDefinition_get_Triggers(definition, &triggers));
    CHECK_COM(ITriggerCollection_get_Count(triggers, &trigger_count));
    CHECK_COM(ITriggerCollection_get_Item(triggers, 1, &trigger));
    CHECK_COM(ITrigger_QueryInterface(trigger, &logon_interface, (void **)&logon));
    CHECK_COM(ILogonTrigger_get_Delay(logon, &delay));
    if (restart_count != 3 || !interval || wcscmp(interval, L"PT1M") != 0 ||
            !limit || wcscmp(limit, L"PT0S") != 0 ||
            battery_disallow != VARIANT_FALSE || battery_stop != VARIANT_FALSE ||
            instances != TASK_INSTANCES_IGNORE_NEW || trigger_count != 1 ||
            !delay || wcscmp(delay, L"PT15S") != 0) {
        fputs("Registered task settings do not provide reliable background startup\n", stderr);
        goto cleanup;
    }
    CHECK_COM(IRegisteredTask_put_Enabled(task, VARIANT_FALSE));
    result = 1;
cleanup:
#undef CHECK_COM
    SysFreeString(delay);
    SysFreeString(limit);
    SysFreeString(interval);
    SysFreeString(name);
    SysFreeString(root);
    if (logon) ILogonTrigger_Release(logon);
    if (trigger) ITrigger_Release(trigger);
    if (triggers) ITriggerCollection_Release(triggers);
    if (settings) ITaskSettings_Release(settings);
    if (definition) ITaskDefinition_Release(definition);
    if (task) IRegisteredTask_Release(task);
    if (folder) ITaskFolder_Release(folder);
    if (service) ITaskService_Release(service);
    CoUninitialize();
    return result;
}

static int alter_task_configuration(const wchar_t *task_name, bool remove_triggers) {
    static const GUID scheduler_class =
        {0x0f87369f, 0xa4e5, 0x4cfc, {0xbd, 0x3e, 0x73, 0xe6, 0x15, 0x45, 0x72, 0xdd}};
    static const GUID scheduler_interface =
        {0x2faba4c7, 0x4da9, 0x4013, {0x96, 0x97, 0x20, 0xcc, 0x3f, 0xd4, 0x0f, 0x85}};
    static const GUID exec_interface =
        {0x4c3d624d, 0xfd6b, 0x49a3, {0xb9, 0xb7, 0x09, 0xcb, 0x3c, 0xd3, 0xf0, 0x47}};
    ITaskService *service = NULL;
    ITaskFolder *folder = NULL;
    IRegisteredTask *task = NULL, *replacement = NULL;
    ITaskDefinition *definition = NULL;
    ITriggerCollection *triggers = NULL;
    IActionCollection *actions = NULL;
    IAction *action = NULL;
    IExecAction *exec = NULL;
    VARIANT empty;
    BSTR root = NULL, name = NULL, arguments = NULL;
    int result = 0;
    HRESULT initialized = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (!require_success(initialized, "mutation COM initialization")) return 0;
    VariantInit(&empty);
    root = SysAllocString(L"\\");
    name = SysAllocString(task_name);
    arguments = SysAllocString(L"--self-test");
    if (!root || !name || !arguments) goto cleanup;
#define CHECK_COM(operation) do { if (!require_success((operation), #operation)) goto cleanup; } while (0)
    CHECK_COM(CoCreateInstance(&scheduler_class, NULL, CLSCTX_INPROC_SERVER,
            &scheduler_interface, (void **)&service));
    CHECK_COM(ITaskService_Connect(service, empty, empty, empty, empty));
    CHECK_COM(ITaskService_GetFolder(service, root, &folder));
    CHECK_COM(ITaskFolder_GetTask(folder, name, &task));
    CHECK_COM(IRegisteredTask_get_Definition(task, &definition));
    if (remove_triggers) {
        CHECK_COM(ITaskDefinition_get_Triggers(definition, &triggers));
        CHECK_COM(ITriggerCollection_Clear(triggers));
    } else {
        CHECK_COM(ITaskDefinition_get_Actions(definition, &actions));
        CHECK_COM(IActionCollection_get_Item(actions, 1, &action));
        CHECK_COM(IAction_QueryInterface(action, &exec_interface, (void **)&exec));
        CHECK_COM(IExecAction_put_Arguments(exec, arguments));
    }
    CHECK_COM(ITaskFolder_RegisterTaskDefinition(folder, name, definition,
            TASK_CREATE_OR_UPDATE, empty, empty, TASK_LOGON_INTERACTIVE_TOKEN,
            empty, &replacement));
    result = 1;
cleanup:
#undef CHECK_COM
    SysFreeString(arguments);
    SysFreeString(name);
    SysFreeString(root);
    if (replacement) IRegisteredTask_Release(replacement);
    if (exec) IExecAction_Release(exec);
    if (action) IAction_Release(action);
    if (actions) IActionCollection_Release(actions);
    if (triggers) ITriggerCollection_Release(triggers);
    if (definition) ITaskDefinition_Release(definition);
    if (task) IRegisteredTask_Release(task);
    if (folder) ITaskFolder_Release(folder);
    if (service) ITaskService_Release(service);
    CoUninitialize();
    return result;
}

int main(int argc, char **argv) {
    wchar_t task_name[120], temp_path[MAX_PATH], directory[MAX_PATH];
    wchar_t executable[MAX_PATH], original[MAX_PATH];
    bool enabled = true;
    int result = 1;
    (void)argv;
    if (argc != 2 || strcmp(argv[1], "--live") != 0) {
        puts("Use --live to exercise Task Scheduler with a temporary independent task.");
        return 2;
    }
    swprintf(task_name, sizeof(task_name) / sizeof(task_name[0]),
            L"WorkBuddyAutostartTest-%lu-%llu", GetCurrentProcessId(), GetTickCount64());
    if (!GetTempPathW(MAX_PATH, temp_path) ||
            !GetModuleFileNameW(NULL, original, MAX_PATH)) return 1;
    swprintf(directory, MAX_PATH, L"%lsWB startup & test-%lu-%llu", temp_path,
            GetCurrentProcessId(), GetTickCount64());
    swprintf(executable, MAX_PATH, L"%ls\\bridge & startup.exe", directory);
    if (!CreateDirectoryW(directory, NULL) || !CopyFileW(original, executable, TRUE)) {
        fprintf(stderr, "Temporary executable setup failed: %lu\n", GetLastError());
        goto cleanup;
    }
    if (!require_success(wb_host_autostart_query(task_name, executable, &enabled),
            "query missing task") || enabled) goto cleanup;
    if (!require_success(wb_host_autostart_set(task_name, executable, false),
            "disable missing task")) goto cleanup;
    if (!require_success(wb_host_autostart_set(task_name, executable, true),
            "enable task")) goto cleanup;
    if (!require_success(wb_host_autostart_query(task_name, executable, &enabled),
            "query enabled task") || !enabled) goto cleanup;
    if (!require_success(wb_host_autostart_query(task_name,
            L"C:\\Windows\\wrong-bridge.exe", &enabled), "query incorrect executable") ||
            enabled) goto cleanup;
    if (!alter_task_configuration(task_name, false)) goto cleanup;
    if (!require_success(wb_host_autostart_query(task_name, executable, &enabled),
            "query task with non-startup arguments") || enabled) {
        fputs("A task running --self-test must not count as autostart\n", stderr);
        goto cleanup;
    }
    if (!require_success(wb_host_autostart_set(task_name, executable, true),
            "restore original arguments")) goto cleanup;
    if (!alter_task_configuration(task_name, true)) goto cleanup;
    if (!require_success(wb_host_autostart_query(task_name, executable, &enabled),
            "query task without logon trigger") || enabled) {
        fputs("A task without logon triggers must not count as autostart\n", stderr);
        goto cleanup;
    }
    if (!require_success(wb_host_autostart_set(task_name, executable, true),
            "restore logon trigger")) goto cleanup;
    if (!verify_registered_configuration_and_disable(task_name)) goto cleanup;
    if (!require_success(wb_host_autostart_query(task_name, executable, &enabled),
            "query externally disabled task") || enabled) goto cleanup;
    if (!require_success(wb_host_autostart_set(task_name, executable, true),
            "enable task idempotently")) goto cleanup;
    if (!require_success(wb_host_autostart_query(task_name, executable, &enabled),
            "query restored task") || !enabled) goto cleanup;
    if (!require_success(wb_host_autostart_set(task_name, executable, false),
            "disable task")) goto cleanup;
    if (!require_success(wb_host_autostart_query(task_name, executable, &enabled),
            "query disabled task") || enabled) goto cleanup;
    if (!require_success(wb_host_autostart_set(task_name, executable, false),
            "disable task idempotently")) goto cleanup;
    if (wb_host_autostart_query(NULL, executable, &enabled) != E_INVALIDARG ||
            wb_host_autostart_query(task_name, executable, NULL) != E_INVALIDARG ||
            wb_host_autostart_set(task_name, L"relative.exe", true) != E_INVALIDARG) {
        fputs("Invalid arguments must return E_INVALIDARG\n", stderr);
        goto cleanup;
    }
    puts("HostAutostartIntegrationTest: PASS (enable, exact action path, XML ampersand, "
            "external disable, startup/restart/battery settings, disable)");
    result = 0;
cleanup:
    if (!require_success(wb_host_autostart_set(task_name, executable, false),
            "cleanup test task")) result = 1;
    DeleteFileW(executable);
    RemoveDirectoryW(directory);
    return result;
}
