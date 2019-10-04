// TDLInfoTipCtrl.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "TDLInfoTipCtrl.h"
#include "TDCEnumContainers.h"
#include "ToDoCtrlDataDefines.h"
#include "ToDoCtrlData.h"
#include "ToDoCtrlDataUtils.h"

#include "..\Shared\Misc.h"
#include "..\Shared\GraphicsMisc.h"
#include "..\Shared\Localizer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////

static const CString EMPTY_STR;

/////////////////////////////////////////////////////////////////////////////
// CTDLInfoTipCtrl

CTDLInfoTipCtrl::CTDLInfoTipCtrl()
{
}

CTDLInfoTipCtrl::~CTDLInfoTipCtrl()
{
}


BEGIN_MESSAGE_MAP(CTDLInfoTipCtrl, CToolTipCtrlEx)
	//{{AFX_MSG_MAP(CTDLInfoTipCtrl)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
	ON_WM_CREATE()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTDLInfoTipCtrl message handlers

int CTDLInfoTipCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CToolTipCtrlEx::OnCreate(lpCreateStruct) != 0)
		return -1;

	CLocalizer::EnableTranslation(GetSafeHwnd(), FALSE);
	return 0;
}

CString CTDLInfoTipCtrl::FormatTip(DWORD dwTaskID,
								   const CTDCAttributeMap& mapAttrib,
								   const CToDoCtrlData& data,
								   const CTDCCustomAttribDefinitionArray& aCustAttribs,
								   int nMaxCommentsLen) const
{
	// Build the attribute array
	CTDCInfoTipItemArray aItems;

	if (!BuildSortedAttributeArray(dwTaskID, mapAttrib, data, aCustAttribs, nMaxCommentsLen, aItems))
		return EMPTY_STR;

	// Calculate offset to make item values line up
	CClientDC dc(const_cast<CTDLInfoTipCtrl*>(this));
	CFont* pOldFont = GraphicsMisc::PrepareDCFont(&dc, GetSafeHwnd());

	// Calculate the pixel width of the longest label plus a tab
	// Note: Labels already incorporate first tab
	int nMaxWidth = 0;
	int nItem = aItems.GetSize();
	CRect rItem(0, 0, 10000, 100);

	while (nItem--)
	{
		TDCINFOTIPITEM& iti = aItems[nItem];
		dc.DrawText(iti.sLabel, rItem, DT_EXPANDTABS | DT_CALCRECT);
		
		iti.nLabelWidth = rItem.Width();
		nMaxWidth = max(nMaxWidth, iti.nLabelWidth);
	}

	// Now add tabs to each label until they all have the same length as the maximum
	nItem = aItems.GetSize();

	while (nItem--)
	{
		TDCINFOTIPITEM& iti = aItems[nItem];

		while (iti.nLabelWidth < nMaxWidth)
		{
			iti.sLabel += '\t';
			dc.DrawText(iti.sLabel, rItem, DT_EXPANDTABS | DT_CALCRECT);

			iti.nLabelWidth = rItem.Width();
		}
	}
	
	// Build the final string
	CString sTip;

	for (int nItem = 0; nItem < aItems.GetSize(); nItem++)
	{
		sTip += aItems[nItem].sLabel;
		sTip += aItems[nItem].sValue;
		sTip += _T("\n");
	}

	dc.SelectObject(pOldFont);

	return sTip;
}

int CTDLInfoTipCtrl::BuildSortedAttributeArray(DWORD dwTaskID, 
											   const CTDCAttributeMap& mapAttrib,
											   const CToDoCtrlData& data,
											   const CTDCCustomAttribDefinitionArray& aCustAttribs,
											   int nMaxCommentsLen,
											   CTDCInfoTipItemArray& aItems) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;

	if (!data.GetTrueTask(dwTaskID, pTDI, pTDS))
		return 0;

	CTDCTaskFormatter formatter(data);
	
	// Always add title
	aItems.Add(TDCINFOTIPITEM(TDCA_TASKNAME, IDS_TDLBC_TASK, pTDI->sTitle));

	// A few attributes require special handling
	if (mapAttrib.Has(TDCA_COMMENTS) && !pTDI->sComments.IsEmpty())
	{
		CString sComments = pTDI->sComments;
		int nLen = sComments.GetLength();

		if (nMaxCommentsLen != 0)
		{
			if ((nMaxCommentsLen > 0) && (nLen > nMaxCommentsLen))
				sComments = (sComments.Left(nMaxCommentsLen) + _T("..."));
		}

		aItems.Add(TDCINFOTIPITEM(TDCA_COMMENTS, IDS_TDLBC_COMMENTS, sComments));
	}

	// Rest are simple
#define ADDINFOITEM(a, s, e)						\
	if (mapAttrib.Has(a))							\
	{												\
		CString sVal = e;							\
		if (!sVal.IsEmpty())						\
			aItems.Add(TDCINFOTIPITEM(a, s, sVal));	\
	}

	ADDINFOITEM(TDCA_ALLOCBY, IDS_TDLBC_ALLOCBY, pTDI->sAllocBy);
	ADDINFOITEM(TDCA_ALLOCTO, IDS_TDLBC_ALLOCTO, formatter.GetTaskAllocTo(pTDI));
	ADDINFOITEM(TDCA_CATEGORY, IDS_TDLBC_CATEGORY, formatter.GetTaskCategories(pTDI));
	ADDINFOITEM(TDCA_COST, IDS_TDLBC_COST, formatter.GetTaskCost(pTDI, pTDS));
	ADDINFOITEM(TDCA_CREATEDBY, IDS_TDLBC_CREATEDBY, pTDI->sCreatedBy);
	ADDINFOITEM(TDCA_CREATIONDATE, IDS_TDLBC_CREATEDATE, FormatDate(pTDI->dateCreated, data));
	ADDINFOITEM(TDCA_DEPENDENCY, IDS_TDLBC_DEPENDS, Misc::FormatArray(pTDI->aDependencies));
	ADDINFOITEM(TDCA_DONEDATE, IDS_TDLBC_DONEDATE, FormatDate(pTDI->dateDone, data));
	ADDINFOITEM(TDCA_FLAG, IDS_TDLBC_FLAG, CEnString(pTDI->bFlagged ? IDS_YES : 0));
	ADDINFOITEM(TDCA_LASTMODBY, IDS_TDLBC_LASTMODBY, pTDI->sLastModifiedBy);
	ADDINFOITEM(TDCA_LASTMODDATE, IDS_TDLBC_LASTMODDATE, FormatDate(pTDI->dateLastMod, data));
	ADDINFOITEM(TDCA_LOCK, IDS_TDLBC_LOCK, CEnString(pTDI->bLocked ? IDS_YES : 0));
	ADDINFOITEM(TDCA_PERCENT, IDS_TDLBC_PERCENT, formatter.GetTaskPercentDone(pTDI, pTDS));
	ADDINFOITEM(TDCA_POSITION, IDS_TDLBC_POS, formatter.GetTaskPosition(pTDS));
	ADDINFOITEM(TDCA_PRIORITY, IDS_TDLBC_PRIORITY, formatter.GetTaskPriority(pTDI, pTDS));
	ADDINFOITEM(TDCA_RECURRENCE, IDS_TDLBC_RECURRENCE, formatter.GetTaskRecurrence(pTDI));
	ADDINFOITEM(TDCA_RISK, IDS_TDLBC_RISK, formatter.GetTaskRisk(pTDI, pTDS));
	ADDINFOITEM(TDCA_STATUS, IDS_TDLBC_STATUS, pTDI->sStatus);
	ADDINFOITEM(TDCA_SUBTASKDONE, IDS_TDLBC_SUBTASKDONE, formatter.GetTaskSubtaskCompletion(pTDI, pTDS));
	ADDINFOITEM(TDCA_TAGS, IDS_TDLBC_TAGS, formatter.GetTaskTags(pTDI));
	ADDINFOITEM(TDCA_TIMEEST, IDS_TDLBC_TIMEEST, formatter.GetTaskTimeEstimate(pTDI, pTDS));
	ADDINFOITEM(TDCA_TIMESPENT, IDS_TDLBC_TIMESPENT, formatter.GetTaskTimeSpent(pTDI, pTDS));
	ADDINFOITEM(TDCA_VERSION, IDS_TDLBC_VERSION, pTDI->sVersion);

	if (pTDI->aFileLinks.GetSize())
		ADDINFOITEM(TDCA_FILEREF, IDS_TDLBC_FILEREF, pTDI->aFileLinks[0]);

	if (!pTDI->IsDone() || !data.HasStyle(TDCS_HIDESTARTDUEFORDONETASKS))
	{
		ADDINFOITEM(TDCA_STARTDATE, IDS_TDLBC_STARTDATE, FormatDate(pTDI->dateStart, data));
		ADDINFOITEM(TDCA_DUEDATE, IDS_TDLBC_DUEDATE, FormatDate(pTDI->dateDue, data));
	}

	if (mapAttrib.Has(TDCA_DEPENDENCY))
	{
		CDWordArray aDependents;

		if (data.GetTaskLocalDependents(dwTaskID, aDependents))
			ADDINFOITEM(TDCA_DEPENDENCY, IDS_TDLBC_DEPENDENTS, Misc::FormatArray(aDependents));
	}

	// Custom attributes
	// TODO

	// Alphabetic Sort
	Misc::SortArrayT(aItems, InfoTipSortProc);

	return aItems.GetSize();
}

int CTDLInfoTipCtrl::InfoTipSortProc(const void* pV1, const void* pV2)
{
	const TDCINFOTIPITEM* pITI1 = (const TDCINFOTIPITEM*)pV1;
	const TDCINFOTIPITEM* pITI2 = (const TDCINFOTIPITEM*)pV2;

	// Task title always sorts at top
	if (pITI1->nAttribID == TDCA_TASKNAME)
		return -1;

	if (pITI2->nAttribID == TDCA_TASKNAME)
		return 1;

	// Comments always sort at bottom
	if (pITI1->nAttribID == TDCA_COMMENTS)
		return 1;

	if (pITI2->nAttribID == TDCA_COMMENTS)
		return -1;

	// Rest sort by label
	return Misc::NaturalCompare(pITI1->sLabel, pITI2->sLabel);
}

CString CTDLInfoTipCtrl::FormatDate(const COleDateTime& date, const CToDoCtrlData& data) const
{
	if (!CDateHelper::IsDateSet(date))
		return EMPTY_STR;

	DWORD dwDateFmt = data.HasStyle(TDCS_SHOWDATESINISO) ? DHFD_ISO : 0;

	if (CDateHelper::DateHasTime(date))
		dwDateFmt |= DHFD_TIME | DHFD_NOSEC;

	return CDateHelper::FormatDate(date, dwDateFmt);
}