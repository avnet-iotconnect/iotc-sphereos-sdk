#ifndef	EXIT_CODES_C
#define EXIT_CODES_C

/// <summary>
/// Exit codes for this application. These are used for the
/// application exit code. They must all be between zero and 255,
/// where zero is reserved for successful termination.
/// </summary>
typedef enum {

    ExitCode_Success = 0,
    ExitCode_TermHandler_SigTerm = 1,
    ExitCode_App_Exit = 2,

    ExitCode_Validate_ConnectionType = 20,
    ExitCode_Validate_ScopeId = 21,

    ExitCode_Init_EventLoop = 30,
    ExitCode_Init_IotConnect_Sdk = 31,

    ExitCode_Create_AppTimer = 40,
    ExitCode_AppTimer_Consume = 41,

    ExitCode_Main_EventLoopFail = 50,

    ExitCode_IotConnect_Sdk_EventLoopFail = 70,

} ExitCode;

#endif 