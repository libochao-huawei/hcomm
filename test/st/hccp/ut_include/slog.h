#ifndef __SLOG_H
#define __SLOG_H

#ifdef CCU_CONFIG_LLT
void DlogRecord(int module_id, int level, const char *fmt, ...)
{
}

int CheckLogLevel(int moduleId, int level)
{
    return 1;
}
#endif

enum {
    SLOG,          /**< Slog */
    IDEDD,         /**< IDE daemon device */
    IDEDH,         /**< IDE daemon host */
    HCCL,          /**< HCCL */
    FMK,           /**< Adapter */
    CCU,
    HIAIENGINE,    /**< Matrix */
    DVPP,          /**< DVPP */
    RUNTIME,       /**< Runtime */
    CCE,           /**< CCE */
#if (OS_TYPE == LINUX)
    HDC,         /**< HDC */
#else
    HDCL,
#endif // OS_TYPE
    DRV,           /**< Driver */
    NET,
    MDCFUSION,     /**< Mdc fusion */
    MDCLOCATION,   /**< Mdc location */
    MDCPERCEPTION, /**< Mdc perception */
    MDCFSM,
    MDCCOMMON,
    MDCMONITOR,
    MDCBSWP,    /**< MDC base software platform */
    MDCDEFAULT, /**< MDC undefine */
    MDCSC,      /**< MDC spatial cognition */
    MDCPNC,
    MLL,      /**< abandon */
    DEVMM,    /**< Dlog memory managent */
    KERNEL,   /**< Kernel */
    LIBMEDIA, /**< Libmedia */
    CCECPU,   /**< aicpu shedule */
    ASCENDDK, /**< AscendDK */
    ROS,      /**< ROS */
    HCCP,
    ROCE,
    TEFUSION,
    PROFILING, /**< Profiling */
    DP,        /**< Data Preprocess */
    APP,       /**< User Application */
    TS,        /**< TS module */
    TSDUMP,    /**< TSDUMP module */
    AICPU,     /**< AICPU module */
    LP,        /**< LP module */
    TDT,       /**< tsdaemon or aicpu shedule */
    FE,
    MD,
    MB,
    ME,
    IMU,
    IMP,
    GE, /**< Fmk */
    MDCFUSA,
    CAMERA,
    ASCENDCL,
    TEEOS,
    ISP,
    SIS,
    HSM,
    DSS,
    PROCMGR,     // Process Manager, Base Platform
    BBOX,
    AIVECTOR,
    TBE,
    FV,
    MDCMAP,
    TUNE,
    HSS, /**< helper */
    FFTS,
    OP,
    UDF,
    HICAID,
    TSYNC,
    INVLID_MOUDLE_ID
};
#endif