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
 * and the fourth is the ID value that is used in the program for the item
 * The examples at the top illustrate 
 *   To hide the contents of a group box, you must also include the string for the group
 *     box as well as the one(s) for the items in the box
 *   Menu items will be identified with part or all of the menu name, items in dialogs
 *     will have a string based on the dialog
 *   Submenu items will have a string indicating the submenu name
 *   An entire top-level menu can be hidden
 *   Control panels can be hidden
 */

static DisableHideItem sDisableHideList[] =
{
  {"MultishotDlg - Center shots", 2, 1, IDC_RNO_CENTER},  // Must include the group too
  {"MultishotDlg - Center shot group", 2, 0, IDC_STAT_CENTER_GROUP},
  {"MultishotDlg - Second ring", 2, 1, IDC_SPIN_RING2_NUM},
  {"Cal - IS&Stage - Stage Shift", 2, 0, ID_CALIBRATION_STAGESHIFT},  // A sub-menu item
  {"Cal - Mag Energy Shifts", 2, 0, ID_CALIBRATION_MAGENERGYSHIFTS}, // A main menu item
  {"Calibration menu", 2, 0, -1},                                    // An entire menu
  {"Beam & Spot submenu", 2, 0, -1},                                 // An entire submenu
  {"Buffer Status panel", 2, 0, -10},                                // Control panels
  {"Buffer Control panel", 2, 0, -11},
  {"Image Display panel", 2, 0, -12},
  {"Scope Control panel", 2, 0, -14},
  {"Tilt panel", 2, 0, -15},
};