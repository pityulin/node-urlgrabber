#include <napi.h>
#include <Windows.h>
#include <UIAutomation.h>

IUIAutomation* automation;
IUIAutomationCondition* condition;

class InitAutomation {
public:  
    InitAutomation() { };
    HRESULT Init() {
        HRESULT hr = CoInitialize(NULL);
        if SUCCEEDED(hr) {
            hr = CoCreateInstance(__uuidof(CUIAutomation), NULL,
                CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, __uuidof(IUIAutomation),
                (void**)&automation);
            if SUCCEEDED(hr) {
                VARIANT prop;
                prop.vt = VT_BOOL;
                prop.boolVal = VARIANT_TRUE;
                hr = automation->CreatePropertyCondition(UIA_IsControlElementPropertyId, prop, &condition);
            }
        }

        return hr;
    }

    ~InitAutomation() {
        if (condition) {
            condition->Release();
        }
        if (automation) {
            automation->Release();
        }
        CoUninitialize();
    };
} __initAutomation;


std::wstring EnumerateChildren(IUIAutomationElement* parentElement, TreeScope scope, IUIAutomationCondition* pCondition) {
    IUIAutomationElementArray* elemArr = NULL;
    std::wstring url = L"";

    HRESULT hr = parentElement->FindAll(scope, pCondition, &elemArr);

    if (SUCCEEDED(hr) && elemArr) {
        int length = 0;
        
        hr = elemArr->get_Length(&length);
        if SUCCEEDED(hr) {
            for (int i = 1; i < length; i++) {
                IUIAutomationElement* elem = NULL;
                
                hr = elemArr->GetElement(i, &elem);
                if (SUCCEEDED(hr) && elem) {
                    CONTROLTYPEID contolType = 0;
                    
                    hr = elem->get_CurrentControlType(&contolType);
                    if (SUCCEEDED(hr) && (contolType == UIA_EditControlTypeId || contolType == UIA_GroupControlTypeId || contolType == UIA_DocumentControlTypeId)) {
                        IUnknown* pattern = NULL;

                        hr = elem->GetCurrentPattern(UIA_ValuePatternId, &pattern);
                        elem->Release();
                        if (SUCCEEDED(hr) && pattern) {
                            IUIAutomationValuePattern* valPattern = NULL;

                            hr = pattern->QueryInterface(IID_IUIAutomationValuePattern, (LPVOID*)&valPattern);
                            pattern->Release();
                            if (SUCCEEDED(hr) && valPattern != NULL) {
                                BSTR value;

                                hr = valPattern->get_CurrentValue(&value);
                                valPattern->Release();
                                if SUCCEEDED(hr) {
                                    std::wstring wStr(value, SysStringLen(value));
                                    url = wStr;
                                    SysFreeString(value);
                                    break;
                                }
                            }
                        }
                    }
                    else {
                        url = EnumerateChildren(elem, scope, pCondition);
                        if (!url.empty()) {
                            break;
                        }
                    }
                }
            }
        }
    }
    if (elemArr) {
        elemArr->Release();
    }

    return url;
}


Napi::Value getUrl(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() != 1) {
        Napi::TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[0].IsNumber()) {
        Napi::TypeError::New(env, "Wrong arguments type").ThrowAsJavaScriptException();
        return env.Null();
    }

    __int64 arg0 = info[0].As<Napi::Number>().Int64Value();

    if (arg0 <= 0) {
        Napi::TypeError::New(env, "Invalid Handle").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!automation || !condition) {
        if (condition) {
            condition->Release();
        }
        if (automation) {
            automation->Release();
        }

        CoUninitialize();

        Napi::TypeError::New(env, "Error on Create UIAutomation").ThrowAsJavaScriptException();
        return env.Null();
    }

    IUIAutomationElement* elem = NULL;
    Napi::String url = Napi::String::New(env, "");

    HRESULT hr = automation->ElementFromHandle((HWND)arg0, &elem);
    if (SUCCEEDED(hr) && elem)
    {
        std::wstring wStr = EnumerateChildren(elem, (TreeScope)(TreeScope::TreeScope_Element | TreeScope::TreeScope_Children), condition);
        if (!wStr.empty()) {
            std::u16string str(wStr.begin(), wStr.end());
            url = Napi::String::New(env, str);
        }
    }
    if (elem) {
        elem->Release();
    }
    return url;
}



Napi::Object Init(Napi::Env env, Napi::Object exports) {
    __initAutomation.Init();
    exports.Set(Napi::String::New(env, "getUrl"), Napi::Function::New(env, getUrl));
    return exports;
}

NODE_API_MODULE(cMath, Init)