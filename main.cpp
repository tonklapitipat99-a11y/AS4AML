#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/config.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>

#include <angelscript.h>

#include <datetime/datetime.h>
#include <scriptarray/scriptarray.h>
#include <amlscriptbuilder.h>
#include <scripthandle/scripthandle.h>
#include <scripthelper/scripthelper.h>
#include <scriptmath/scriptmath.h>
#include <scriptstdstring/scriptstdstring.h>

MYMODCFG(net.rusjj.as4aml, AngelScript for AML, 0.2, Supap10)

asIScriptEngine *engine;
void AddAS4AMLFuncs();

// =====================================================================
// >>> ส่วนที่ 1: ฟังชั่นกราฟิกและลูกเล่นใหม่ๆ <<<
// =====================================================================
void SetGameVisualTest() {
    // ให้มันพิมพ์ข้อความแจ้งเตือนลงใน Log แบบเด่นๆ
    logger->Info("=============================================");
    logger->Info("--- [AS4AML] สั่งเปลี่ยนค่ากราฟิกสำเร็จแล้ว! ---");
    logger->Info("=============================================");
}

void MessageCallback(const asSMessageInfo *msg, void *param) {
    if(msg->type == asMSGTYPE_WARNING)
        logger->Print(LogP_Warn, "%s (%d, %d) : %s", msg->section, msg->row, msg->col, msg->message);
    else if(msg->type == asMSGTYPE_INFORMATION) 
        logger->Info("%s (%d, %d) : %s", msg->section, msg->row, msg->col, msg->message);
    else
        logger->Error("%s (%d, %d) : %s", msg->section, msg->row, msg->col, msg->message);
}

void SimplePrint(std::string &msg) {
    asIScriptContext *ctx = asGetActiveContext();
    logger->Info("<%s>: %s", ctx->GetFunction(0)->GetModuleName(), msg.c_str());
}

inline bool EndsWithAS(const char* base) {
    static int blen;
    blen = strlen(base);
    return (blen >= 3) && (!strcmp(base + blen - 3, ".as"));
}

void LoadAS(const char* path) {
    char buf[0xFF];
    DIR* dir = opendir(path);
    if (dir != NULL) {
        struct dirent *diread;
        while ((diread = readdir(dir)) != NULL) {
            if(diread->d_name[0] == '.') continue;
            if(!EndsWithAS(diread->d_name)) continue;

            int r = builder->StartNewModule(engine, diread->d_name);
            if(r < 0) { logger->Error("Failed to start module!"); continue; }
            
            sprintf(buf, "%s/%s", path, diread->d_name);
            r = builder->AddSectionFromFile(buf);
            if(r < 0) { logger->Error("Failed to load script!"); continue; }
            
            asIScriptFunction *func = builder->BuildAMLModule()->GetFunctionByDecl("void main()");
            if(func) {
                asIScriptContext *ctx = engine->CreateContext();
                ctx->Prepare(func);
                r = ctx->Execute();
                if( r == asEXECUTION_EXCEPTION ) logger->Error("Exception: %s", ctx->GetExceptionString());
                ctx->Release();
            }
            logger->Info("Loaded Script: %s", diread->d_name);
        }
        closedir(dir);
    }
}

extern "C" void OnModPreLoad() {
    logger->SetTag("AS4AML_Supap");
    logger->Info("Starting AS4AML for %s", aml->GetCurrentGame());
    
    engine = asCreateScriptEngine();
    if(!engine) return;
    
    engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);
    
    RegisterStdString(engine);
    RegisterScriptDateTime(engine);
    RegisterScriptArray(engine, true);
    RegisterStdStringUtils(engine);
    RegisterScriptHandle(engine);
    RegisterExceptionRoutines(engine);
    RegisterScriptMath(engine);
    
    // ลงทะเบียนคำสั่งไว้ให้สคริปต์ใช้
    engine->RegisterGlobalFunction("void print(const string &in)", asFUNCTION(SimplePrint), asCALL_CDECL);
    engine->RegisterGlobalFunction("void SetVisual()", asFUNCTION(SetGameVisualTest), asCALL_CDECL);
    
    AddAS4AMLFuncs();
    
    // Register an interface
    RegisterInterface("AngelScript", engine);
    RegisterInterface("AS4AML", as4aml);
}

extern "C" void OnAllModsLoaded() {
    char buf[0xFF];
    const char* gameId = aml->GetCurrentGame();

    // ตรวจสอบโฟลเดอร์ Unprotected ของคุณ
    logger->Info("Checking for scripts in Unprotected folder...");
    sprintf(buf, "/storage/emulated/0/Android_Unprotected/data/%s/angelscript", gameId);
    mkdir(buf, 0777);
    LoadAS(buf);

    // ตรวจสอบโฟลเดอร์ปกติ
    sprintf(buf, "/sdcard/Android/data/%s/angelscript", gameId);
    mkdir(buf, 0777);
    LoadAS(buf);
}

extern "C" void OnModUnload() { 
    if(engine) engine->Release(); 
}

static AMLScriptBuilder builderLocal;
AMLScriptBuilder* builder = &builderLocal;
