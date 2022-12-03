#pragma once

struct DisableHideItem {
  const char *descrip;
  short disableOrHide;
  short wholeLine;
  int nID;
};

/*
 * This is the table of items currently eligible to be disabled and/or hidden
 * In the disable or hide file, each line needs two entries:
 *  1 to disable an item or 2 to hide it; and the text string listed below, without the
 *  quotes.
 * For example:
 *   2 Calibration menu
 *   2 MultishotDlg - Second ring
 * These items can be included both in a file of items to permanently disable or hide for
 * the session of the program, and in a file defining items to disable or hide in basic
 * mode.
 * In the table below, the second value is:
 *    1 if the item can be disabled but not hidden
 *    2 if the item can be hidden but not disabled
 *    3 if the item can be both disabled and hidden
 * The third entry in the table is 1 if a whole line of items is hidden together,
 * and the fourth is the ID value that is used in titem
 * The examples at the top illustrate 
 *   To hide the contents of a group box, you must also include the string for the group
 *     box as well as the one(s) for the items in the box
 *   Menu items will be identified with part or all of the menu name, items in dialogs
 *     will have a string based on the dialog
 *   Submenu items will have a string indicating the submenu name
 *   An entire top-level menu can be hidden
 *   Control panels can be hidden, or their options sections
 *
 * There are two options for setting up hiding, either use the panel tables or 
 * have a simple list of IDs to hide and call ManageHideableItems
 * A control panel with the latter must have an UpdateHiding override that is called
 * on initialization and by SetBasicMode to call ManageHideableItems, but one using panel 
 * tables does not need to since the base function in CToolDlg handles it.
 * A non-modal dialog with either mechanism must have an UpdateHiding function that is 
 * called on initialization and by SetBasicMode.
 * Be sure mInitialized is set to true and that it is NOT a subclass member
 * To hide a whole line, make a table entry for the highest item on the line.
 * If there is a group box, it has to be hidden separately, the spacing doesn't work
 * to use it to hide all its contents.
 */
 
static DisableHideItem sDisableHideList[] =
{
  {"MultishotDlg - Center shots", 2, 1, IDC_RNO_CENTER},  // Must include the group too
  {"MultishotDlg - Center shot group", 2, 0, IDC_STAT_CENTER_GROUP},
  {"MultishotDlg - Second ring", 2, 1, IDC_SPIN_RING2_NUM},
  {"Cal - IS&Stage - Stage Shift", 2, 0, ID_CALIBRATION_STAGESHIFT},  // A sub-menu item
  {"Cal - Mag Energy Shifts", 2, 0, ID_CALIBRATION_MAGENERGYSHIFTS}, // A main menu item
  {"Calibration menu", 2, 0, -1},                                    // An entire menu
  {"Tilt Series menu", 2, 0, -1},
  // Whole submenus
  {"Beam & Spot submenu", 2, 0, -1},
  {"Pixel Size submenu", 2, 0, -1},
  {"Focus & Tuning submenu", 2, 0, -1},
  {"Correlation Filter submenu", 2, 0, -1},
  {"Special Settings submenu", 2, 0, -1},
  {"Old Coma-Free Alignment submenu", 2, 0, -1},    // In Calibration Focus & Tune submenu
  {"Old Coma-free Alignment submenu", 2, 0, -1},    // In Focus & Tune menu
  {"Specialized Options submenu", 2, 0, -1},
  {"Nav. Options submenu", 2, 0, -1},

  // Whole control panels
  {"Buffer Status panel", 2, 0, -10},                                
  {"Buffer Control panel", 2, 0, -11},
  {"Image Display panel", 2, 0, -12},
  {"Scope Control panel", 2, 0, -14},
  {"Tilt panel", 2, 0, -15},
  {"Low Dose panel", 2, 0, -18},
  {"Montage panel", 2, 0, -19},
  // Options sections in control panels
  {"Buffer Control options", 2, 0, -31},
  {"Image Display options", 2, 0, -32},
  {"Scope Status options", 2, 0, -33},
  {"Tilt panel options", 2, 0, -35},
  {"Image Align options", 2, 0, -37},
  {"Low Dose options", 2, 0, -38},
  {"Montage Control options", 2, 0, -39},
  {"STEM Control options", 2, 0, -40},
  {"Filter Control options", 2, 0, -41},
  {"DE Camera options", 2, 0, -42},
  // Low dose panel
  {"LowDoseDlg - Continuous update", 2, 0, IDC_CONTINUOUSUPDATE},
  {"LowDoseDlg - Added beam Shift", 2, 1, IDC_SET_BEAM_SHIFT},  // Must include the group too
  {"LowDoseDlg - Added beam group", 2, 0, IDC_STAT_ADD_SHIFT_BOX},
  // Offsets section: first three will hide it completely
  {"LowDoseDlg - Offsets View", 2, 1, IDC_RVIEW_OFFSET},
  {"LowDoseDlg - Offsets Def spin", 2, 1, IDC_SPINVIEWDEFOCUS},
  {"LowDoseDlg - Offsets line 5", 2, 0, IDC_STAT_LDCP_LINE5},
  {"LowDoseDlg - Offsets group", 2, 0, IDC_STAT_VS_OFFSETS}, // Otherwise use entries for single items
  {"LowDoseDlg - Offsets line 1", 2, 0, IDC_STAT_LDCP_LINE1},
  {"LowDoseDlg - Offsets line 2", 2, 0, IDC_STAT_LDCP_LINE2},
  {"LowDoseDlg - Offsets line 3", 2, 0, IDC_STAT_LDCP_LINE3},
  {"LowDoseDlg - Offsets line 4", 2, 0, IDC_STAT_LDCP_LINE4},
  {"LowDoseDlg - Offsets Search", 2, 0, IDC_RSEARCH_OFFSET},
  {"LowDoseDlg - Offsets Shift label", 2, 0, IDC_STAT_VS_SHIFT},
  {"LowDoseDlg - Offsets Set", 2, 0, IDC_SET_VIEW_SHIFT},
  {"LowDoseDlg - Offsets Zero", 2, 0, IDC_ZERO_VIEW_SHIFT},
  {"LowDoseDlg - Offsets Def label", 2, 0, IDC_STATVIEWDEFLABEL},
  {"LowDoseDlg - Offsets Def value", 2, 0, IDC_STATVIEWDEFOCUS},
  // Camera & Script panel
  {"CameraScript - Focus", 2, 0, ID_CAMERA_FOCUS},
  {"CameraScript - Trial", 2, 0, ID_CAMERA_TRIAL},
  {"CameraScript - Spinner 1", 2, 0, IDC_SPINMACRO1},
  {"CameraScript - Spinner 2", 2, 0, IDC_SPINMACRO2},
  {"CameraScript - Spinner 3", 2, 0, IDC_SPINMACRO3},
  {"CameraScript - Spinner 4", 2, 0, IDC_SPINMACRO4},
  {"CameraScript - Spinner 5", 2, 0, IDC_SPINMACRO5},
  {"CameraScript - Spinner 6", 2, 0, IDC_SPINMACRO6},
  {"CameraScript - Spinner 7", 2, 0, IDC_SPINMACRO7},
  {"CameraScript - Spinner 8", 2, 0, IDC_SPINMACRO8},
  {"CameraScript - Spinner 9", 2, 0, IDC_SPINMACRO9},
  {"CameraScript - Spinner 10", 2, 0, IDC_SPINMACRO10},
  {"CameraScript - Spinner 11", 2, 0, IDC_SPINMACRO11},
  {"CameraScript - Spinner 12", 2, 0, IDC_SPINMACRO12},
  // Image Alignment & Focus panel
  {"ImageAlignFocus - Align", 2, 1, IDC_BUTALIGN},
  {"ImageAlignFocus - Clear", 2, 0, IDC_BUTCLEARALIGN},
  {"ImageAlignFocus - To marker", 2, 0, IDC_BUT_TO_MARKER},
  // Montage control panel
  {"MontControl - Prescan", 2, 0, IDC_BTRIAL},
  // Filter Control panel   (hiding will leave empty space)
  {"FilterControl - TEM screen down", 2, 0, IDC_AUTOMAG},
  // STEM Control panel   (hiding will leave empty space)
  {"STEMControl - TEM screen down", 2, 0, IDC_SCREEN_SWITCH_STEM},
  // Montage Setup Dialog
  {"MontSetupDlg - Piece size", 2, 1, IDC_EDITXSIZE},
  {"MontSetupDlg - Overlap", 2, 1, IDC_EDITXOVERLAP},
  {"MontSetupDlg - Min overlap label", 2, 1, IDC_EDITMINOVERLAP},
  {"MontSetupDlg - Move stage", 2, 0, IDC_MOVESTAGE}, // Someone could get stuck with this
  {"MontSetupDlg - Skip pieces check", 2, 0, IDC_CHECK_SKIP_OUTSIDE},
  {"MontSetupDlg - Skip pieces edit", 2, 0, IDC_EDIT_SKIP_OUTSIDE},
  {"MontSetupDlg - Do full rect", 2, 0, IDC_IGNORESKIPS},
  {"MontSetupDlg - Ask about map", 2, 0, IDC_CHECKOFFERMAP},  // Now just "Make map..."
  {"MontSetupDlg - Close when done", 2, 0, IDC_CHECK_CLOSE_WHEN_DONE},
  {"MontSetupDlg - Use mont-map", 2, 0, IDC_CHECK_USE_MONT_MAP_PARAMS},
  {"MontSetupDlg - Use View", 2, 0, IDC_CHECK_USE_VIEW_IN_LOWDOSE},
  {"MontSetupDlg - Use Search", 2, 0, IDC_CHECK_USE_SEARCH_IN_LD},
  {"MontSetupDlg - Use multishot", 2, 0, IDC_CHECK_USE_MULTISHOT},
  {"MontSetupDlg - Continuous check", 2, 0, IDC_CHECK_CONTINUOUS_MODE},
  {"MontSetupDlg - Continuous edit", 2, 0, IDC_EDIT_CONTIN_DELAY_FAC},
  {"MontSetupDlg - Turn off drift", 2, 0, IDC_CHECK_NO_DRIFT_CORR},
  {"MontSetupDlg - Use high quality", 2, 0, IDC_CHECK_USE_HQ_SETTINGS},
  // Imaging State Dialog  
  {"ImageStateDlg - Add current", 2, 0, IDC_BUT_ADDCURSTATE},
  {"ImageStateDlg - Add Nav", 2, 0, IDC_BUT_ADDNAVSTATE},
  {"ImageStateDlg - Delete", 2, 0, IDC_BUT_DELSTATE},
  {"ImageStateDlg - Update", 2, 0, IDC_BUT_UPDATE_STATE},
  {"ImageStateDlg - Name label", 2, 0, IDC_STAT_STATE_NAME},
  {"ImageStateDlg - Name edit", 2, 0, IDC_EDIT_STATENAME},
  {"ImageStateDlg - Map acquire", 2, 0, IDC_BUT_SETMAPSTATE},
  {"ImageStateDlg - Scheduled", 2, 0, IDC_BUT_SETSCHEDSTATE},
  {"ImageStateDlg - Restore", 2, 0, IDC_BUT_RESTORESTATE},
  {"ImageStateDlg - Forget", 2, 0, IDC_BUT_FORGETSTATE},
  {"ImageStateDlg - Save target", 2, 0, IDC_BUT_SAVE_DEFOCUS},
  {"ImageStateDlg - Go to state", 2, 0, IDC_BUT_SETIMSTATE},
  {"ImageStateDlg - Add mont-map", 2, 0, IDC_BUT_ADD_MONT_MAP},
  // Navigator Dialog
  {"Navigator - Corner", 2, 0, IDC_CHECKCORNER},
  {"Navigator - New map", 2, 0, IDC_NEW_MAP},
  {"Navigator - Anchor map", 2, 0, IDC_BUT_DUAL_MAP},
  {"Navigator - Rotate", 2, 0, IDC_CHECKROTATE},
  {"Navigator - Anchor state", 2, 0, IDC_CHECK_DUALMAP},
  {"Navigator - Draw labels", 2, 0, IDC_DRAW_LABELS},
  {"Navigator - Draw none", 2, 0, IDC_DRAW_NONE},
  {"Navigator - Draw all reg", 2, 0, IDC_DRAW_ALL_REG},
  {"Navigator - Draw text", 2, 0, IDC_STAT_NAV_DRAW},   // "Draw" to left of checkboxes
  {"Navigator - Set text", 2, 0, IDC_STAT_NAV_SET},    // "Set" to left of buttons
  {"Navigator - File properties", 2, 0, IDC_BUT_NAV_FILEPROPS},
  {"Navigator - State", 2, 0, IDC_BUT_NAV_STATE},
  {"Navigator - TS params", 2, 0, IDC_BUT_NAV_TSPARAMS},
  {"Navigator - Filename", 2, 0, IDC_BUT_NAV_FILENAME},
  {"Navigator - Focus pos", 2, 0, IDC_BUT_NAV_FOCUS_POS},
  // Frame File Options Dialog
  {"FrameFileOpts - Save MRC", 2, 0, IDC_SAVE_MRC},
  {"FrameFileOpts - Use .mrcs", 2, 0, IDC_USE_EXTENSION_MRCS},
  {"FrameFileOpts - LZW compressed", 2, 0, IDC_COMPRESSED_TIFF},
  {"FrameFileOpts - ZIP compressed", 2, 0, IDC_TIFF_ZIP_COMPRESS},
  {"FrameFileOpts - File type", 2, 0, IDC_STAT_FILETYPE}, // The group box
  {"FrameFileOpts - One frame/file", 2, 0, IDC_ONE_FRAME_PER_FILE},  // This is Falcon 4 EER mode
  {"FrameFileOpts - Frame stack mdoc", 2, 0, IDC_FRAME_STACK_MDOC},
  {"FrameFileOpts - Save unnormalized", 2, 0, IDC_SAVE_UNNORMALIZED},
  {"FrameFileOpts - No rotation/flip", 2, 0, IDC_SKIP_ROTFLIP},
};
