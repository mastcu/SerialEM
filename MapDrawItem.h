// MapDrawItem.h: interface for the CMapDrawItem class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MAPDRAWITEM_H__66DFD065_E39E_4A47_9CD0_02E9013B2469__INCLUDED_)
#define AFX_MAPDRAWITEM_H__66DFD065_E39E_4A47_9CD0_02E9013B2469__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define ITEM_TYPE_POINT 0
#define ITEM_TYPE_POLYGON 1
#define ITEM_TYPE_MAP 2
#define NO_RAW_STAGE -10000.f
#define RAW_STAGE_TEST -9990.f
#define NO_REALIGN_COLOR 5
#define DEFAULT_POINT_COLOR 0
#define DEFAULT_POLY_COLOR 1
#define DEFAULT_MAP_COLOR 2
#define DEFAULT_SUPER_COLOR 3
#define POINT_ACQUIRE_COLOR 6
#define MAP_ACQUIRE_COLOR   7
#define HIGHLIGHT_COLOR     8

class CMapDrawItem  
{
public:
	COLORREF GetColor(bool highlight);
	void DeletePoint(int index);
	void AppendPoint(float inX, float inY);
	void AddPoint(float inX, float inY, int index);
	CMapDrawItem();
	virtual ~CMapDrawItem();

  int mNumPoints;         // Number of points
  int mMaxPoints;         // Current size of point array
  float *mPtX;            // Arrays for X and Y point coordinates
  float *mPtY;
  CString mLabel;         // Label string drawn on screen
  int mColor;             // Color index
  CString mNote;          // Long note string
  float mStageX;          // Stage position
  float mStageY;
  float mStageZ;
  float mRawStageX;       // Unadjusted stage position
  float mRawStageY;
  BOOL mCorner;           // Flag for corner point
  BOOL mDraw;             // Flag to draw
  BOOL mAcquire;          // Flag to acquire area
  BOOL mRotOnLoad;        // Flag to rotate when load
  int mRegPoint;          // Registration point number
  int mRegistration;      // Registration at which item exists
  int mOriginalReg;       // Original registration
  int mOldReg;            // Registration before last transform
  int mType;              // Type = point, polygon, map
  CString mMapFile;       // Full name of file where map lives
  CString mTrimmedName;   // Trimmed name
  BOOL mMapMontage;       // Flag that map is a montage
  int mMapSection;        // Section number in file
  int mMapBinning;        // Binning at which map was taken or of initial map image
  int mMontBinning;       // Actual binning of montage used to make the map
  int mMontUseStage;      // 1 if montage was taken with stage, 0 if not, -1 if unknown
  float mBacklashX;       // Backlash when a montage was taken or for position generally
  float mBacklashY;
  int mMapMagInd;         // Magnification
  int mMapCamera;         // Camera
  float mMapExposure;     // Exposure time for original map images
  float mMapSettling;     // Drift settling
  int mShutterMode;       // Shutter mode
  int mK2ReadMode;        // Read mode for K2 camera
  float mDefocusOffset;   // Defocus offset for map based on View images
  float mNetViewShiftX;   //   And net IS offset for a View image map
  float mNetViewShiftY;
  float mViewBeamShiftX;  // Incremental beam shift and tilt to apply for a View image map
  float mViewBeamShiftY;
  float mViewBeamTiltX;
  float mViewBeamTiltY;
  int mMapAlpha;          // Alpha value on JEOL
  int mMapID;             // Unique ID number
  int mGroupID;           // Unique ID for supermontage or other grouping
  int mPolygonID;         // ID of polygon used to define supermont
  int mDrawnOnMapID;      // ID of map point/polygon was drawn on
  int mAtSamePosID;       // Items with this matching were taken at same raw stage pos
  int mPieceDrawnOn;      // Index of piece in map that item was drawn on
  ScaleMat mMapScaleMat;  // Scale matrix for drawing
  int mMapWidth;          // Size of image at which scale matrix was defined
  int mMapHeight;
  int mTextExtent;
  float mMapMinScale;     // Min and max scale values of image when map was defined
  float mMapMaxScale;
  int mMapFramesX;        // Number of montage frames when montage acquired
  int mMapFramesY;
  int mMapSpotSize;       // Illumination state when map was taken
  double mMapIntensity;
  int mMapProbeMode;
  float mMapTiltAngle;    // Tilt angle of image
  BOOL mMapSlitIn;        // Filter state when map was taken; ignore energy loss
  float mMapSlitWidth;
  int mMapLowDoseConSet;
  int mRealignedID;       // ID of lower mag map with nearby realign error
  float mRealignErrX;     // Final stage error of that realign operation
  float mRealignErrY;
  float mLocalRealiErrX;  // Error in second round of realign operation
  float mLocalRealiErrY;
  int mRealignReg;        // Original registration of that realign
  int mImported;          // Indicator of an imported map or point drawn on one
  int mOldImported;       // Value of import flag before transform
  int mImageType;         // STORE_TYPE flag for image
  int mSuperMontX;        // Camera coordinate of this montage in supermontage
  int mSuperMontY;
  int mMoveStageID;       // Temporary flag to keep track of item stage was moved to
  int mFilePropIndex;     // Indexes into arrays of file properties and other params
  int mTSparamIndex;
  int mMontParamIndex;
  int mStateIndex;
  CString mFileToOpen;    // Filename of file to open at this item
};

#endif // !defined(AFX_MAPDRAWITEM_H__66DFD065_E39E_4A47_9CD0_02E9013B2469__INCLUDED_)
