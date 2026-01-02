// ManyChoiceDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "ManyChoiceDlg.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

static int sIdTable[] = { IDC_MANYCHOICEHEADER,
IDC_GENERIC_RADIO1, IDC_GENERIC_CHECK1, IDC_GENERIC_RADIO2, IDC_GENERIC_CHECK2,
IDC_GENERIC_RADIO3, IDC_GENERIC_CHECK3, IDC_GENERIC_RADIO4, IDC_GENERIC_CHECK4,
IDC_GENERIC_RADIO5, IDC_GENERIC_CHECK5, IDC_GENERIC_RADIO6, IDC_GENERIC_CHECK6,
IDC_GENERIC_RADIO7, IDC_GENERIC_CHECK7, IDC_GENERIC_RADIO8, IDC_GENERIC_CHECK8,
IDC_GENERIC_RADIO9, IDC_GENERIC_CHECK9, IDC_GENERIC_RADIO10, IDC_GENERIC_CHECK10,
IDC_GENERIC_RADIO11, IDC_GENERIC_CHECK11, IDC_GENERIC_RADIO12, IDC_GENERIC_CHECK12,
IDC_GENERIC_RADIO13, IDC_GENERIC_CHECK13, IDC_GENERIC_RADIO14, IDC_GENERIC_CHECK14,
IDC_GENERIC_RADIO15, IDC_GENERIC_CHECK15, PANEL_END, IDOK, IDCANCEL, IDC_BUTHELP,
PANEL_END, TABLE_END };

static int sTopTable[sizeof(sIdTable) / sizeof(int)];
static int sLeftTable[sizeof(sIdTable) / sizeof(int)];
static int sHeightTable[sizeof(sIdTable) / sizeof(int)];


// CManyChoiceDlg dialog

CManyChoiceDlg::CManyChoiceDlg(CWnd* pParent /*=NULL*/)
  : CBaseDlg(IDD_MANYCHOICEBOX, pParent)
{
  mIsRadio = false;
  mNumChoices = MAX_CHOICES;
  mHeader = _T("");
  mRadioVal = 0;
  for (int i = 0; i < mNumChoices; i++) {
    mCheckboxVals[i] = FALSE;
    mChoiceLabels[i] = _T("");
  }
}

CManyChoiceDlg::~CManyChoiceDlg()
{
}

void CManyChoiceDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_MANYCHOICEHEADER, mHeader);
  for (int id = IDC_GENERIC_CHECK1, i = 0; i < MAX_CHOICES; id++, i++) {
    DDX_Check(pDX, id, mCheckboxVals[i]);
  }
  DDX_Radio(pDX, IDC_GENERIC_RADIO1, mRadioVal);
}


BEGIN_MESSAGE_MAP(CManyChoiceDlg, CBaseDlg)
  ON_COMMAND_RANGE(IDC_GENERIC_RADIO1, IDC_GENERIC_RADIO15, OnRadio)
  ON_COMMAND_RANGE(IDC_GENERIC_CHECK1, IDC_GENERIC_CHECK15, OnCheck)
END_MESSAGE_MAP()


// CManyChoiceDlg message handlers

BOOL CManyChoiceDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  BOOL states[2] = { true, true };
  int IDstart, otherIDstart;

  SetupPanelTables(sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart,
    sHeightTable);
  mIDsToDrop.push_back(IDC_BUTHELP);
  if (mIsRadio) {
    IDstart = IDC_GENERIC_RADIO1;
    otherIDstart = IDC_GENERIC_CHECK1;
  }
  else {
    IDstart = IDC_GENERIC_CHECK1;
    otherIDstart = IDC_GENERIC_RADIO1;
  }

  // Hide controls of other type
  for (int i = otherIDstart; i < otherIDstart + MAX_CHOICES; i++) {
    mIDsToDrop.push_back(i);
  }
  for (int i = IDstart, ind = 0; i < IDstart + MAX_CHOICES; i++, ind++) {
    if (ind < mNumChoices)
      // Edit the button label
      SetDlgItemText(i, _T(mChoiceLabels[ind]));
    else
      // Hide extra options that will be unused
      mIDsToDrop.push_back(i);
  }

  AdjustPanels(states, sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart, 0,
   sHeightTable);
  return TRUE;
}


void CManyChoiceDlg::OnRadio(UINT nID)
{
  UpdateData(true);
}

void CManyChoiceDlg::OnCheck(UINT nID)
{
  UpdateData(true);
}
