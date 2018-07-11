/*
 * This file has definitions shared between SerialEM and SerialEMCCD
 * The "master" should be in SerialEMCCD, although it is checked in from both
 */
#ifndef SEMCCD_DEFINES_H

#define SEMCCD_PLUGIN_VERSION    109

// Error codes
enum {IMAGE_NOT_FOUND = 1, WRONG_DATA_TYPE, DM_CALL_EXCEPTION, NO_STACK_ID, STACK_NOT_3D,
FILE_OPEN_ERROR, SEEK_ERROR, WRITE_DATA_ERROR, HEADER_ERROR, ROTBUF_MEMORY_ERROR, 
DIR_ALREADY_EXISTS, DIR_CREATE_ERROR, DIR_NOT_EXIST, SAVEDIR_IS_FILE, DIR_NOT_WRITABLE,
FILE_ALREADY_EXISTS, QUIT_DURING_SAVE, OPEN_DEFECTS_ERROR, WRITE_DEFECTS_ERROR, 
THREAD_ERROR, EARLY_RET_WITH_SYNC, CONTINUOUS_ENDED, BAD_SUM_LIST, BAD_ANTIALIAS_PARAM,
CLIENT_SCRIPT_ERROR, GENERAL_SCRIPT_ERROR, GETTING_DEFECTS_ERROR,
DS_CHANNEL_NOT_ACQUIRED, NO_DEFERRED_SUM, NO_GPU_AVAILABLE, DEFECT_PARSE_ERROR, 
GAIN_REF_LOAD_ERROR, BAD_FRAME_REDUCE_PARAM, FRAMEALI_INITIALIZE, FRAMEALI_NEXT_FRAME,
FRAMEALI_FINISH_ALIGN, MAKECOM_BAD_PARAM, MAKECOM_NO_REL_PATH, OPEN_COM_ERROR,
WRITE_COM_ERROR, OPEN_MDOC_ERROR, WRITE_MDOC_ERROR, COPY_MDOC_ERROR, FRAMEALI_BAD_SUBSET};

// Flags for SetupFileSaving
#define K2_SAVE_RAW_PACKED       1
#define K2_COPY_GAIN_REF   (1 << 1)
#define K2_RUN_COMMAND     (1 << 2)
#define K2_SAVE_LZW_TIFF   (1 << 3)
#define K2_SAVE_ZIP_TIFF   (1 << 4)
#define K2_SAVE_SYNCHRON   (1 << 5)
#define K2_SAVE_DEFECTS    (1 << 6)
#define K2_EARLY_RETURN    (1 << 7)
#define K2_ASYNC_IN_RAM    (1 << 8)
#define K2_SKIP_FRAME_ROTFLIP  (1 << 9)
#define K2_SAVE_SUMMED_FRAMES  (1 << 10)
#define K2_GAIN_NORM_SUM       (1 << 11)
#define K2_SAVE_4BIT_MRC_MODE  (1 << 12)
#define K2_RAW_COUNTING_4BIT   (1 << 13)
#define K2_MAKE_DEFERRED_SUM   (1 << 14)
#define K2_SAVE_TIMES_100      (1 << 15)
#define K2_MRCS_EXTENSION      (1 << 16)
#define K2_SAVE_SUPER_REDUCED  (1 << 17)

// Flags for SetK2Parameters
#define K2_ANTIALIAS_MASK      7
#define K2_OVW_MAKE_SUBAREA    (1 << 3)
#define K2_USE_FRAMEALIGN      (1 << 4)
#define K2_TAKE_BINNED_FRAMES  (1 << 5)
#define K2_MAKE_ALIGN_COM      (1 << 11)
#define K2_REDUCED_Y_SCALE     100000.

// Flags for SetupFrameAligning (bit 1 is apply gain ref, bit 7 is for early return)
// (And bit 5 is for synchronous align/save, and bit 6 for apply defects, 
// 8 for async in RAM, 11 for making an align com)
#define K2FA_USE_HYBRID_SHIFTS       1
#define K2FA_SMOOTH_SHIFTS     (1 << 2)
#define K2FA_GROUP_REFINE      (1 << 3)
#define K2FA_DEFER_GPU_SUM     (1 << 4)
#define K2FA_MAKE_EVEN_ODD     (1 << 9)
#define K2FA_KEEP_PRECISION    (1 << 10)
#define K2FA_ALIGN_SUBSET      (1 << 12)

#define K2FA_SUB_START_MASK    0x3F
#define K2FA_SUB_END_SHIFT     6

#define K2FA_FRC_INT_SCALE     1000000.

// Flags for MakeAlignComFile
#define K2FA_WRITE_MDOC_TEXT          1

// DM's flag for drift correction
#define OVW_DRIFT_CORR_FLAG   0x1000

// Flags for AcquireDSImage in lineSync argument
#define DS_LINE_SYNC             1
#define DS_BEAM_TO_SAFE    (1 << 1)
#define DS_BEAM_TO_FIXED   (1 << 2)
#define DS_BEAM_TO_EDGE    (1 << 3)
#define DS_CONTROL_SCAN    (1 << 4)

// Continuous mode definitions
#define QUALITY_BITS_SHIFT   3
#define QUALITY_BITS_MASK    7
#define CONTINUOUS_USE_THREAD  (1 << 6)
#define CONTINUOUS_SET_MODE    (1 << 7)
#define CONTINUOUS_ACQUIS_OBJ  (1 << 8)
#define CONTINUOUS_RETURN_TIMEOUT  2000

// Codes for the socket calls
enum {GS_ExecuteScript = 1, GS_SetDebugMode, GS_SetDMVersion, GS_SetCurrentCamera,
      GS_QueueScript, GS_GetAcquiredImage, GS_GetDarkReference, GS_GetGainReference,
      GS_SelectCamera, GS_SetReadMode, GS_GetNumberOfCameras, GS_IsCameraInserted,
      GS_InsertCamera, GS_GetDMVersion, GS_GetDMCapabilities,
      GS_SetShutterNormallyClosed, GS_SetNoDMSettling, GS_GetDSProperties,
      GS_AcquireDSImage, GS_ReturnDSChannel, GS_StopDSAcquisition, GS_CheckReferenceTime,
      GS_SetK2Parameters, GS_ChunkHandshake, GS_SetupFileSaving, GS_GetFileSaveResult,
      GS_SetupFileSaving2, GS_GetDefectList, GS_SetK2Parameters2, GS_StopContinuousCamera,
      GS_GetPluginVersion, GS_GetLastError, GS_FreeK2GainReference, GS_IsGpuAvailable,
      GS_SetupFrameAligning, GS_FrameAlignResults, GS_ReturnDeferredSum, 
      GS_MakeAlignComFile, GS_WaitUntilReady
};

#endif
