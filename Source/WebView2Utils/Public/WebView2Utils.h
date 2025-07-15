#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FWebView2UtilsModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void* DllHandle=nullptr;
};
