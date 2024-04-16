// MdocDefines.h  - Definitions for most items written to mdoc files
//
// For FLOAT or TWO_FLOATS, entries are the one or two variable names in the EMimageExtra
// class, an initialization value, a value to test if it is greater than before writing it,
// a symbol name and the string written
// For INTEGER, entries are the variable name, initialization value, symbol name and string
// For STRING, entries are the variable name, symbol name and string
// Macros are defined before including this and undefined after in
// EMimageExtra.cpp/h and KStoreAdoc.ccp/h

MDOC_FLOAT(m_fTilt, EXTRA_NO_VALUE, EXTRA_VALUE_TEST, ADOC_TILT, "TiltAngle")
MDOC_TWO_FLOATS(mStageX, mStageY, EXTRA_NO_VALUE, EXTRA_VALUE_TEST, ADOC_STAGE, "StagePosition")
MDOC_FLOAT(mStageZ, EXTRA_NO_VALUE, EXTRA_VALUE_TEST, ADOC_STAGEZ, "StageZ")
MDOC_INTEGER(m_iMag, 0, ADOC_MAG, "Magnification")
MDOC_FLOAT(mCameraLength, 0., 0., ADOC_CAMLEN, "CameraLength")
MDOC_FLOAT(m_fIntensity, -1., -0.1, ADOC_INTENSITY, "Intensity")
MDOC_FLOAT(m_fDose, -1., -0.1, ADOC_DOSE, "ExposureDose")
MDOC_FLOAT(mDoseRate, -1., -0.1, ADOC_DOSE_RATE, "DoseRate")
MDOC_FLOAT(mPixel, 0., 0., ADOC_PIXEL, "PixelSpacing")
MDOC_INTEGER(mSpotSize, 0, ADOC_SPOT, "SpotSize")
MDOC_INTEGER(mProbeMode, -1, ADOC_PROBE_MODE, "ProbeMode")
MDOC_FLOAT(mDefocus, EXTRA_NO_VALUE, EXTRA_VALUE_TEST, ADOC_DEFOCUS, "Defocus")
MDOC_TWO_FLOATS(mISX, mISY, EXTRA_NO_VALUE, EXTRA_VALUE_TEST, ADOC_IMAGESHIFT, "ImageShift")
MDOC_TWO_FLOATS(mNominalStageX, mNominalStageY, EXTRA_NO_VALUE, EXTRA_VALUE_TEST, ADOC_NOMINAL_STAGE, "NominalStageXY")
MDOC_FLOAT(mAxisAngle, EXTRA_NO_VALUE, EXTRA_VALUE_TEST, ADOC_AXIS, "RotationAngle")
MDOC_FLOAT(mExposure, -1., -0.1, ADOC_EXPOSURE, "ExposureTime")
MDOC_FLOAT(mBinning, 0., 0., ADOC_BINNING, "Binning")
MDOC_INTEGER(mCamera, -1, ADOC_CAMERA, "CameraIndex")
MDOC_INTEGER(mDividedBy2, -1, ADOC_DIVBY2, "DividedBy2")
MDOC_INTEGER(mReadMode, -1, ADOC_READ_MODE, "OperatingMode")
MDOC_INTEGER(mCorrDblSampMode, -1, ADOC_CDS_MODE, "UsingCDS")
MDOC_INTEGER(mMagIndex, -1, ADOC_MAGIND, "MagIndex")
MDOC_INTEGER(mLowDoseConSet, -999, ADOC_CONSET, "LowDoseConSet")
MDOC_FLOAT(mCountsPerElectron, -1., -0.1, ADOC_COUNT_ELEC, "CountsPerElectron")
MDOC_FLOAT(mTargetDefocus, EXTRA_NO_VALUE, EXTRA_VALUE_TEST, ADOC_TARGET, "TargetDefocus")
MDOC_FLOAT(mPriorRecordDose, -1, -0.1, ADOC_PRIOR_DOSE, "PriorRecordDose")
MDOC_STRING(mSubFramePath, ADOC_FRAME_PATH, "SubFramePath")
MDOC_INTEGER(mNumSubFrames, -1, ADOC_NUM_FRAMES, "NumSubFrames")
MDOC_STRING(mFrameDosesCounts, ADOC_DOSES_COUNTS, "FrameDosesAndNumber")
MDOC_STRING(mDateTime, ADOC_DATE_TIME, "DateTime")
MDOC_STRING(mNavLabel, ADOC_NAV_LABEL, "NavigatorLabel")
MDOC_TWO_FLOATS(slitWidth, energyLoss, -1., -0.1, ADOC_SLIT_LOSS, "FilterSlitAndLoss")
MDOC_STRING(mChannelName, ADOC_CHAN_NAME, "ChannelName")
MDOC_STRING(mMultiHoleNum, ADOC_MULTI_POS, "MultishotHoleAndPosition")
MDOC_FLOAT(mCamPixSize, 0., 0., ADOC_CAM_PIXEL, "CameraPixelSize")
MDOC_FLOAT(mVoltage, 0., 0., ADOC_VOLTAGE, "Voltage")
MDOC_TWO_INTS(mUncroppedX, mUncroppedY, 0, ADOC_UNCROPPED, "UncroppedSize")
MDOC_INTEGER(mCenteredCrop, -1, ADOC_CENTER_CROP, "CenteredCrop");
MDOC_INTEGER(mRotationAndFlip, -1, ADOC_ROTFLIP, "RotationAndFlip")
MDOC_INTEGER(mSecondTimeStamp, 0, ADOC_SEC_STAMP, "TimeStamp")
MDOC_INTEGER(mFlashCounter, -1, ADOC_FLASH_COUNT, "FlashCounter")
MDOC_FLOAT(mFEGCurrent, -1., -0.1, ADOC_FEG_CURRENT, "FEGCurrent")

// DE12 items
MDOC_STRING(mDE12Version, ADOC_DE12_VERSION, "DE12-ServerSoftwareVersion")
MDOC_FLOAT(mPreExposeTime, -1., -0.1, ADOC_DE12_PREEXPOSE, "DE12-PreexposureTime(s)")
MDOC_INTEGER(mNumDE12Frames, -1, ADOC_DE12_TOTAL_FRAMES, "DE12-TotalNumberOfFrames")
MDOC_FLOAT(mDE12FPS, -1., -0.1, ADOC_DE12_FPS, "DE12-FramesPerSecond")
MDOC_STRING(mDE12Position, ADOC_CAM_POSITION, "DE12-CameraPosition")
MDOC_STRING(mDE12CoverMode, ADOC_COVER_MODE, "DE12-ProtectionCoverMode")
MDOC_INTEGER(mCoverDelay, -1, ADOC_COVER_DELAY, "DE12-ProtectionCoverOpenDelay(ms)")
MDOC_FLOAT(mTemperature, EXTRA_NO_VALUE, EXTRA_VALUE_TEST, ADOC_TEMPERATURE, "DE12-TemperatureDetector(C)")
MDOC_FLOAT(mFaraday, EXTRA_NO_VALUE, EXTRA_VALUE_TEST, ADOC_FARADAY, "DE12-FaradayPlatePeakReading(pA/cm2)")
MDOC_STRING(mSensorSerial, ADOC_SENSOR_SERIAL, "DE12-SensorModuleSerialNumber")
MDOC_INTEGER(mReadoutDelay, -1, ADOC_READOUT_DELAY, "DE12-SensorReadoutDelay(ms)")
MDOC_INTEGER(mIgnoredFrames, -1, ADOC_IGNORED_FRAMES, "DE12-IgnoredFramesInSummedImage")
