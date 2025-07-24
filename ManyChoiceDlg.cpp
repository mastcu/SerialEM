// ManyChoiceDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "ManyChoiceDlg.h"

static int sIdTable[] = { IDC_MANYCHOICEHEADER, 
IDC_GENERIC_RADIO1, IDC_GENERIC_CHECK1, IDC_GENERIC_RADIO2, IDC_GENERIC_CHECK2,
IDC_GENERIC_RADIO3, IDC_GENERIC_CHECK3, IDC_GENERIC_RADIO4, IDC_GENERIC_CHECK4,
IDC_GENERIC_RADIO5, IDC_GENERIC_CHECK5, IDC_GENERIC_RADIO6, IDC_GENERIC_CHECK6,
IDC_GENERIC_RADIO7, IDC_GENERIC_CHECK7, IDC_GENERIC_RADIO8, IDC_GENERIC_CHECK8,
IDC_GENERIC_RADIO9, IDC_GENERIC_CHECK9, IDC_GENERIC_RADIO10, IDC_GENERIC_CHECK10,
IDC_GENERIC_RADIO11, IDC_GENERIC_CHECK11, IDC_GENERIC_RADIO12, IDC_GENERIC_CHECK12,
IDC_GENERIC_RADIO13, IDC_GENERIC_CHECK13, IDC_GENERIC_RADIO14, IDC_GENERIC_CHECK14,
IDC_GENERIC_RADIO15, IDC_GENERIC_CHECK15, PANEL_END, IDOK, IDCANCEL, IDC_BUTHELP, PANEL_END,
TABLE_END };

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
  m_radioVal = 0;
	for (int i = 0; i < mNumChoices; i++) {
		m_checkboxVals[i] = FALSE;
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
	DDX_Check(pDX, IDC_GENERIC_CHECK1, m_checkboxVals[0]);
	DDX_Check(pDX, IDC_GENERIC_CHECK2, m_checkboxVals[1]);
	DDX_Check(pDX, IDC_GENERIC_CHECK3, m_checkboxVals[2]);
	DDX_Check(pDX, IDC_GENERIC_CHECK4, m_checkboxVals[3]);
	DDX_Check(pDX, IDC_GENERIC_CHECK5, m_checkboxVals[4]);
	DDX_Check(pDX, IDC_GENERIC_CHECK6, m_checkboxVals[5]);
	DDX_Check(pDX, IDC_GENERIC_CHECK7, m_checkboxVals[6]);
	DDX_Check(pDX, IDC_GENERIC_CHECK8, m_checkboxVals[7]);
	DDX_Check(pDX, IDC_GENERIC_CHECK9, m_checkboxVals[8]);
	DDX_Check(pDX, IDC_GENERIC_CHECK10, m_checkboxVals[9]);
	DDX_Check(pDX, IDC_GENERIC_CHECK11, m_checkboxVals[10]);
	DDX_Check(pDX, IDC_GENERIC_CHECK12, m_checkboxVals[11]);
	DDX_Check(pDX, IDC_GENERIC_CHECK13, m_checkboxVals[12]);
	DDX_Check(pDX, IDC_GENERIC_CHECK14, m_checkboxVals[13]);
	DDX_Check(pDX, IDC_GENERIC_CHECK15, m_checkboxVals[14]);
  DDX_Radio(pDX, IDC_GENERIC_RADIO1, m_radioVal);
  
}


BEGIN_MESSAGE_MAP(CManyChoiceDlg, CBaseDlg)
	ON_BN_CLICKED(IDC_GENERIC_RADIO1, &CManyChoiceDlg::OnBnClickedGenericRadio1)
	ON_BN_CLICKED(IDC_GENERIC_RADIO2, &CManyChoiceDlg::OnBnClickedGenericRadio2)
	ON_BN_CLICKED(IDC_GENERIC_RADIO3, &CManyChoiceDlg::OnBnClickedGenericRadio3)
	ON_BN_CLICKED(IDC_GENERIC_RADIO4, &CManyChoiceDlg::OnBnClickedGenericRadio4)
	ON_BN_CLICKED(IDC_GENERIC_RADIO5, &CManyChoiceDlg::OnBnClickedGenericRadio5)
	ON_BN_CLICKED(IDC_GENERIC_RADIO6, &CManyChoiceDlg::OnBnClickedGenericRadio6)
	ON_BN_CLICKED(IDC_GENERIC_RADIO7, &CManyChoiceDlg::OnBnClickedGenericRadio7)
	ON_BN_CLICKED(IDC_GENERIC_RADIO8, &CManyChoiceDlg::OnBnClickedGenericRadio8)
	ON_BN_CLICKED(IDC_GENERIC_RADIO10, &CManyChoiceDlg::OnBnClickedGenericRadio10)
	ON_BN_CLICKED(IDC_GENERIC_RADIO11, &CManyChoiceDlg::OnBnClickedGenericRadio11)
	ON_BN_CLICKED(IDC_GENERIC_RADIO12, &CManyChoiceDlg::OnBnClickedGenericRadio12)
	ON_BN_CLICKED(IDC_GENERIC_RADIO13, &CManyChoiceDlg::OnBnClickedGenericRadio13)
	ON_BN_CLICKED(IDC_GENERIC_RADIO14, &CManyChoiceDlg::OnBnClickedGenericRadio14)
	ON_BN_CLICKED(IDC_GENERIC_RADIO15, &CManyChoiceDlg::OnBnClickedGenericRadio15)
	ON_BN_CLICKED(IDC_GENERIC_CHECK1, &CManyChoiceDlg::OnBnClickedGenericCheck1)
	ON_BN_CLICKED(IDC_GENERIC_CHECK2, &CManyChoiceDlg::OnBnClickedGenericCheck2)
	ON_BN_CLICKED(IDC_GENERIC_CHECK3, &CManyChoiceDlg::OnBnClickedGenericCheck3)
	ON_BN_CLICKED(IDC_GENERIC_CHECK4, &CManyChoiceDlg::OnBnClickedGenericCheck4)
	ON_BN_CLICKED(IDC_GENERIC_CHECK5, &CManyChoiceDlg::OnBnClickedGenericCheck5)
	ON_BN_CLICKED(IDC_GENERIC_CHECK6, &CManyChoiceDlg::OnBnClickedGenericCheck6)
	ON_BN_CLICKED(IDC_GENERIC_CHECK7, &CManyChoiceDlg::OnBnClickedGenericCheck7)
	ON_BN_CLICKED(IDC_GENERIC_CHECK8, &CManyChoiceDlg::OnBnClickedGenericCheck8)
	ON_BN_CLICKED(IDC_GENERIC_CHECK9, &CManyChoiceDlg::OnBnClickedGenericCheck9)
	ON_BN_CLICKED(IDC_GENERIC_CHECK10, &CManyChoiceDlg::OnBnClickedGenericCheck10)
	ON_BN_CLICKED(IDC_GENERIC_CHECK11, &CManyChoiceDlg::OnBnClickedGenericCheck11)
	ON_BN_CLICKED(IDC_GENERIC_CHECK12, &CManyChoiceDlg::OnBnClickedGenericCheck12)
	ON_BN_CLICKED(IDC_GENERIC_CHECK13, &CManyChoiceDlg::OnBnClickedGenericCheck13)
	ON_BN_CLICKED(IDC_GENERIC_CHECK14, &CManyChoiceDlg::OnBnClickedGenericCheck14)
	ON_BN_CLICKED(IDC_GENERIC_CHECK15, &CManyChoiceDlg::OnBnClickedGenericCheck15)
END_MESSAGE_MAP()


// CManyChoiceDlg message handlers

BOOL CManyChoiceDlg::OnInitDialog()
{
	CBaseDlg::OnInitDialog();
	BOOL states[2] = { true, true };
	SetupPanelTables(sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart,
		sHeightTable);
	mIDsToDrop.push_back(IDC_BUTHELP);
	
  if (mIsRadio) {
		// Hide checkboxes if there should be radio options
    for (int i = IDC_GENERIC_CHECK1; i < IDC_GENERIC_CHECK1 + MAX_CHOICES; i++) {
			mIDsToDrop.push_back(i);
		}
    // Hide extra options that will be unused
		for (int i = IDC_GENERIC_RADIO1 + mNumChoices; i < IDC_GENERIC_RADIO1 + MAX_CHOICES; 
     i++) {
			mIDsToDrop.push_back(i);
		}
	}
	else {
		// Hide radios if there should be checkbox options
		for (int i = IDC_GENERIC_RADIO1; i < IDC_GENERIC_RADIO1 + MAX_CHOICES; i++) {
			mIDsToDrop.push_back(i);
		}
		// Hide extra options that will be unused
		for (int i = IDC_GENERIC_CHECK1 + mNumChoices; i < IDC_GENERIC_CHECK1 + MAX_CHOICES; 
     i++) {
			mIDsToDrop.push_back(i);
		}
	}
	AdjustPanels(states, sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart, 0,
   sHeightTable);
  return TRUE;
}


void CManyChoiceDlg::OnBnClickedGenericRadio1()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericRadio2()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericRadio3()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericRadio4()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericRadio5()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericRadio6()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericRadio7()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericRadio8()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericRadio9()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericRadio10()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericRadio11()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericRadio12()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericRadio13()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericRadio14()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericRadio15()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericCheck1()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericCheck2()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericCheck3()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericCheck4()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericCheck5()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericCheck6()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericCheck7()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericCheck8()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericCheck9()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericCheck10()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericCheck11()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericCheck12()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericCheck13()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericCheck14()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}


void CManyChoiceDlg::OnBnClickedGenericCheck15()
{
	// TODO: Add your control notification handler code here
	UpdateData(true);
}

void CManyChoiceDlg::DoCancel()
{
	DestroyWindow();
}

void CManyChoiceDlg::OnOK()
{
	// TODO: Add your control notification handler code here
	CBaseDlg::OnOK();
	DoCancel();
}


void CManyChoiceDlg::OnCancel()
{
	// TODO: Add your control notification handler code here
	CBaseDlg::OnCancel();
	DoCancel();
}
