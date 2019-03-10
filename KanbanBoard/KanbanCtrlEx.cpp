// KanbanTreeList.cpp: implementation of the CKanbanTreeList class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include "KanbanCtrlEx.h"
#include "KanbanColors.h"
#include "KanbanMsg.h"

#include "..\shared\DialogHelper.h"
#include "..\shared\DateHelper.h"
#include "..\shared\holdredraw.h"
#include "..\shared\graphicsMisc.h"
#include "..\shared\autoflag.h"
#include "..\shared\misc.h"
#include "..\shared\filemisc.h"
#include "..\shared\enstring.h"
#include "..\shared\localizer.h"
#include "..\shared\themed.h"
#include "..\shared\winclasses.h"
#include "..\shared\wclassdefines.h"
#include "..\shared\copywndcontents.h"
#include "..\shared\enbitmap.h"

#include "..\Interfaces\iuiextension.h"
#include "..\Interfaces\ipreferences.h"
#include "..\Interfaces\TasklistSchemaDef.h"

#include <float.h> // for DBL_MAX
#include <math.h>  // for fabs()

//////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////

#ifndef GET_WHEEL_DELTA_WPARAM
#	define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))
#endif 

#ifndef CDRF_SKIPPOSTPAINT
#	define CDRF_SKIPPOSTPAINT	(0x00000100)
#endif

//////////////////////////////////////////////////////////////////////

const UINT WM_KCM_SELECTTASK = (WM_APP+10); // WPARAM , LPARAM = Task ID

//////////////////////////////////////////////////////////////////////

const UINT IDC_LISTCTRL = 101;
const UINT IDC_HEADER	= 102;

//////////////////////////////////////////////////////////////////////

static CString EMPTY_STR;

//////////////////////////////////////////////////////////////////////

#define GET_KI_RET(id, ki, ret)	\
{								\
	if (id == 0) return ret;	\
	ki = GetKanbanItem(id);		\
	ASSERT(ki);					\
	if (ki == NULL) return ret;	\
}

#define GET_KI(id, ki)		\
{							\
	if (id == 0) return;	\
	ki = GetKanbanItem(id);	\
	ASSERT(ki);				\
	if (ki == NULL)	return;	\
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CKanbanCtrlEx::CKanbanCtrlEx() 
	:
	m_bSortAscending(-1), 
	m_dwOptions(0),
	m_bDragging(FALSE),
	m_bReadOnly(FALSE),
	m_nNextColor(0),
	m_pSelectedList(NULL),
	m_nTrackAttribute(IUI_NONE),
	m_nSortBy(IUI_NONE),
	m_bSelectTasks(FALSE),
	m_bSettingListFocus(FALSE)
{

}

CKanbanCtrlEx::~CKanbanCtrlEx()
{
}

BEGIN_MESSAGE_MAP(CKanbanCtrlEx, CWnd)
	//{{AFX_MSG_MAP(CKanbanCtrlEx)
	//}}AFX_MSG_MAP
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CREATE()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_HEADER, OnHeaderCustomDraw)
	ON_NOTIFY(TVN_BEGINDRAG, IDC_LISTCTRL, OnBeginDragListItem)
	ON_NOTIFY(TVN_SELCHANGED, IDC_LISTCTRL, OnListItemSelChange)
	ON_NOTIFY(TVN_BEGINLABELEDIT, IDC_LISTCTRL, OnListEditLabel)
	ON_NOTIFY(NM_SETFOCUS, IDC_LISTCTRL, OnListSetFocus)
	ON_WM_SETFOCUS()
	ON_WM_SETCURSOR()
	ON_MESSAGE(WM_SETFONT, OnSetFont)
	ON_MESSAGE(WM_KLCN_CHECKCHANGE, OnListCheckChange)
	ON_MESSAGE(WM_KLCN_GETTASKICON, OnListGetTaskIcon)
	ON_MESSAGE(WM_KCM_SELECTTASK, OnSelectTask)

END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////

BOOL CKanbanCtrlEx::Create(DWORD dwStyle, const RECT &rect, CWnd* pParentWnd, UINT nID)
{
	return CWnd::Create(NULL, NULL, dwStyle, rect, pParentWnd, nID);
}

int CKanbanCtrlEx::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_fonts.Initialise(*this);
	
	ModifyStyleEx(0, WS_EX_CONTROLPARENT, 0);

	if (!m_header.Create(HDS_FULLDRAG | HDS_DRAGDROP | WS_CHILD | WS_VISIBLE, 
						 CRect(lpCreateStruct->x, lpCreateStruct->y, lpCreateStruct->cx, 50),
						 this, IDC_HEADER))
	{
		return -1;
	}

	return 0;
}

void CKanbanCtrlEx::FilterToolTipMessage(MSG* pMsg) 
{
	// List tooltips
	CKanbanListCtrlEx* pList = HitTestListCtrl(pMsg->pt);

	if (pList)
		pList->FilterToolTipMessage(pMsg);
}

bool CKanbanCtrlEx::ProcessMessage(MSG* pMsg) 
{
	switch (pMsg->message)
	{
	case WM_KEYDOWN:
		return (HandleKeyDown(pMsg->wParam, pMsg->lParam) != FALSE);
	}
	
	// all else
	return false;
}

BOOL CKanbanCtrlEx::SelectClosestAdjacentItemToSelection(int nAdjacentList)
{
	if (!m_pSelectedList->GetSelectedCount())
	{
		ASSERT(0);
		return FALSE;
	}

	if ((nAdjacentList < 0) || (nAdjacentList > m_aListCtrls.GetSize()))
	{
		ASSERT(0);
		return FALSE;
	}

	CKanbanListCtrlEx* pAdjacentList = m_aListCtrls[nAdjacentList];

	if (!pAdjacentList->GetCount())
		return FALSE;

	// Find the closest task at the currently
	// selected task's scrolled pos
	HTREEITEM hti = m_pSelectedList->GetSelectedItem();
	ASSERT(hti >= 0);

	// scroll into view first
	m_pSelectedList->EnsureVisible(hti);

	CRect rItem;
	VERIFY(m_pSelectedList->GetItemBounds(hti, &rItem));

	HTREEITEM htiClosest = pAdjacentList->HitTest(rItem.CenterPoint());

	if (!htiClosest)
		htiClosest = pAdjacentList->TCH().GetLastItem();

	SelectListCtrl(pAdjacentList, FALSE);
	ASSERT(m_pSelectedList == pAdjacentList);

	pAdjacentList->SelectItem(htiClosest);
	pAdjacentList->UpdateWindow();

	return TRUE;
}

BOOL CKanbanCtrlEx::SelectNextItem(int nIncrement)
{
/*
	if (nIncrement == 0)
	{
		ASSERT(0);
		return FALSE;
	}

	int nSelItem = m_pSelectedList->GetSelectedItem();
	int nNumItem = m_pSelectedList->GetCount();

	int nNextItem = -1;

	if (nSelItem == -1)
	{
		if (nIncrement > 0)
			nNextItem = 0;
		else
			nNextItem = (nNumItem - 1);
	}
	else
	{
		if (nIncrement > 0)
			nNextItem = min((nSelItem + nIncrement), (nNumItem - 1));
		else
			nNextItem = max((nSelItem + nIncrement), 0);
	}

	if (nNextItem != nSelItem)
	{
		VERIFY(m_pSelectedList->SelectItem(nNextItem, TRUE));
		m_pSelectedList->EnsureVisible(nNextItem);

		return TRUE;
	}
*/

	// else
	return FALSE;
}

BOOL CKanbanCtrlEx::HandleKeyDown(WPARAM wp, LPARAM lp)
{
	switch (wp)
	{
	case VK_LEFT:
		if (m_pSelectedList->GetSelectedCount())
		{
			int nSelList = m_aListCtrls.Find(m_pSelectedList);

			for (int nList = (nSelList - 1); nList >= 0; nList--)
			{
				if (SelectClosestAdjacentItemToSelection(nList))
					return TRUE;
			}
		}
		break;

	case VK_HOME:
		if (m_pSelectedList->GetSelectedCount())
		{
			int nSelList = m_aListCtrls.Find(m_pSelectedList);

			for (int nList = 0; nList < nSelList; nList++)
			{
				if (SelectClosestAdjacentItemToSelection(nList))
					return TRUE;
			}
		}
		break;

	case VK_RIGHT:
		if (m_pSelectedList->GetSelectedCount())
		{
			int nSelList = m_aListCtrls.Find(m_pSelectedList);
			int nNumList = m_aListCtrls.GetSize();

			for (int nList = (nSelList + 1); nList < nNumList; nList++)
			{
				if (SelectClosestAdjacentItemToSelection(nList))
					return TRUE;
			}
		}
		break;

	case VK_END:
		if (m_pSelectedList->GetSelectedCount())
		{
			int nSelList = m_aListCtrls.Find(m_pSelectedList);
			int nNumList = m_aListCtrls.GetSize();

			for (int nList = (nNumList - 1); nList > nSelList; nList--)
			{
				if (SelectClosestAdjacentItemToSelection(nList))
					return TRUE;
			}
		}
		break;

	case VK_ESCAPE:
		// handle 'escape' during dragging
		return (CancelOperation() != FALSE);

	case VK_DELETE:
		if (m_pSelectedList && !m_pSelectedList->IsBacklog())
		{
			// For each of the selected tasks remove the attribute value(s) of the
			// selected list (column). Tasks having no values remaining are moved 
			// to the backlog
			CStringArray aListValues;
			VERIFY(m_pSelectedList->GetAttributeValues(aListValues));

			DWORD dwTaskID = GetSelectedTaskID();
			KANBANITEM* pKI = GetKanbanItem(dwTaskID);
			ASSERT(pKI);

			if (pKI)
				pKI->RemoveTrackedAttributeValues(m_sTrackAttribID, aListValues);

			// Notify parent of changes before altering the lists because we can't
			// guarantee that all the modified tasks will be in the same list afterwards
			NotifyParentAttibuteChange(dwTaskID);

			// Reset selected list before removing items to 
			// to prevent unwanted selection notifications
			CKanbanListCtrlEx* pList = m_pSelectedList;
			m_pSelectedList = NULL;

			if (pKI)
			{
				VERIFY(pList->DeleteTask(dwTaskID));

				if (!pKI->HasTrackedAttributeValues(m_sTrackAttribID))
				{
					CKanbanListCtrlEx* pBacklog = m_aListCtrls.GetBacklog();

					if (pBacklog)
						pBacklog->AddTask(*pKI, FALSE);
				}
			}

			// try to restore selection
			SelectTask(dwTaskID);
			return TRUE;
		}
		break;
	}

	// all else
	return FALSE;
}

BOOL CKanbanCtrlEx::SelectTask(DWORD dwTaskID)
{
	CAutoFlag af(m_bSelectTasks, TRUE);

	// Check for 'no change'
	DWORD dwSelID = GetSelectedTaskID();

	int nPrevSel = m_aListCtrls.Find(m_pSelectedList);
	int nNewSel = m_aListCtrls.Find(dwTaskID);

	if ((nPrevSel == nNewSel) && (dwTaskID == dwSelID))
		return TRUE;

	ClearOtherListSelections(NULL);

	if ((nNewSel == -1) || (dwTaskID == 0))
		return FALSE;

	// else
	SelectListCtrl(m_aListCtrls[nNewSel], FALSE);
	VERIFY(m_pSelectedList->SelectTask(dwTaskID));

	ScrollToSelectedTask();

	return TRUE;
}

DWORD CKanbanCtrlEx::GetSelectedTaskID() const
{
	const CKanbanListCtrlEx* pList = GetSelListCtrl();

	if (pList)
		return pList->GetSelectedTaskID();

	// else
	return 0;
}

CKanbanListCtrlEx* CKanbanCtrlEx::GetSelListCtrl()
{
	ASSERT((m_pSelectedList == NULL) || Misc::HasT(m_aListCtrls, m_pSelectedList));

	if (!m_pSelectedList && m_aListCtrls.GetSize())
		m_pSelectedList = m_aListCtrls[0];

	return m_pSelectedList;
}

const CKanbanListCtrlEx* CKanbanCtrlEx::GetSelListCtrl() const
{
	if (m_pSelectedList)
	{
		return m_pSelectedList;
	}
	else if (m_aListCtrls.GetSize())
	{
		return m_aListCtrls[0];
	}

	// else
	return NULL;
}

BOOL CKanbanCtrlEx::SelectTask(IUI_APPCOMMAND nCmd, const IUISELECTTASK& select)
{
	CKanbanListCtrlEx* pList = NULL;
	HTREEITEM htiStart = NULL;
	BOOL bForwards = TRUE;

	switch (nCmd)
	{
	case IUI_SELECTFIRSTTASK:
		pList = m_aListCtrls.GetFirstNonEmpty();

		if (pList)
			htiStart = pList->TCH().GetFirstItem();
		break;

	case IUI_SELECTNEXTTASK:
		pList = m_pSelectedList;

		if (pList)
			htiStart = pList->GetNextSiblingItem(pList->GetSelectedItem());;
		break;

	case IUI_SELECTNEXTTASKINCLCURRENT:
		pList = m_pSelectedList;

		if (pList)
			htiStart = pList->GetSelectedItem();
		break;

	case IUI_SELECTPREVTASK:
		pList = m_pSelectedList;
		bForwards = FALSE;

		if (pList)
			htiStart = pList->GetPrevSiblingItem(pList->GetSelectedItem());
		break;

	case IUI_SELECTLASTTASK:
		pList = m_aListCtrls.GetLastNonEmpty();
		bForwards = FALSE;

		if (pList)
			htiStart = pList->TCH().GetLastItem();
		break;

	default:
		ASSERT(0);
		break;
	}

	if (pList)
	{
		const CKanbanListCtrlEx* pStartList = pList;
		HTREEITEM hti = htiStart;
		
		do
		{
			hti = pList->FindTask(select, bForwards, hti);

			if (hti)
			{
				SelectListCtrl(pList, FALSE);
				return pList->SelectItem(hti);
			}

			// else
			pList = GetNextListCtrl(pList, bForwards, TRUE);
			hti = (bForwards ? pList->TCH().GetFirstItem() : pList->TCH().GetLastItem());
		}
		while (pList != pStartList);
	}

	// all else
	return false;
}

BOOL CKanbanCtrlEx::HasFocus() const
{
	return CDialogHelper::IsChildOrSame(GetSafeHwnd(), ::GetFocus());
}

void CKanbanCtrlEx::UpdateTasks(const ITaskList* pTaskList, IUI_UPDATETYPE nUpdate, const CSet<IUI_ATTRIBUTE>& attrib)
{
	ASSERT(GetSafeHwnd());

	const ITASKLISTBASE* pTasks = GetITLInterface<ITASKLISTBASE>(pTaskList, IID_TASKLISTBASE);

	if (pTasks == NULL)
	{
		ASSERT(0);
		return;
	}

	// always cancel any ongoing operation
	CancelOperation();

	BOOL bResort = FALSE;
	
	switch (nUpdate)
	{
	case IUI_ALL:
		RebuildData(pTasks, attrib);
 		RebuildListCtrls(TRUE, TRUE);
		break;
		
	case IUI_NEW:
	case IUI_EDIT:
		{
 			// update the task(s)
			BOOL bChange = UpdateGlobalAttributeValues(pTasks, attrib);
			bChange |= UpdateData(pTasks, pTasks->GetFirstTask(), attrib, TRUE);

			if (bChange)
				RebuildListCtrls(TRUE, TRUE);
			else
				RedrawListCtrls(TRUE);
		}
		break;
		
	case IUI_DELETE:
		RemoveDeletedTasks(pTasks);
		break;
		
	default:
		ASSERT(0);
	}
}

int CKanbanCtrlEx::GetTaskAllocTo(const ITASKLISTBASE* pTasks, HTASKITEM hTask, CStringArray& aValues)
{
	aValues.RemoveAll();
	int nItem = pTasks->GetTaskAllocatedToCount(hTask);
	
	if (nItem > 0)
	{
		if (nItem == 1)
		{
			aValues.Add(pTasks->GetTaskAllocatedTo(hTask, 0));
		}
		else
		{
			while (nItem--)
				aValues.InsertAt(0, pTasks->GetTaskAllocatedTo(hTask, nItem));
		}
	}	
	
	return aValues.GetSize();
}

int CKanbanCtrlEx::GetTaskCategories(const ITASKLISTBASE* pTasks, HTASKITEM hTask, CStringArray& aValues)
{
	aValues.RemoveAll();
	int nItem = pTasks->GetTaskCategoryCount(hTask);

	if (nItem > 0)
	{
		if (nItem == 1)
		{
			aValues.Add(pTasks->GetTaskCategory(hTask, 0));
		}
		else
		{
			while (nItem--)
				aValues.InsertAt(0, pTasks->GetTaskCategory(hTask, nItem));
		}
	}	

	return aValues.GetSize();
}

int CKanbanCtrlEx::GetTaskTags(const ITASKLISTBASE* pTasks, HTASKITEM hTask, CStringArray& aValues)
{
	aValues.RemoveAll();
	int nItem = pTasks->GetTaskTagCount(hTask);

	if (nItem > 0)
	{
		if (nItem == 1)
		{
			aValues.Add(pTasks->GetTaskTag(hTask, 0));
		}
		else
		{
			while (nItem--)
				aValues.InsertAt(0, pTasks->GetTaskTag(hTask, nItem));
		}
	}	

	return aValues.GetSize();
}

// External interface
BOOL CKanbanCtrlEx::WantEditUpdate(IUI_ATTRIBUTE nAttrib) const
{
	switch (nAttrib)
	{
	case IUI_ALLOCBY:
	case IUI_ALLOCTO:
	case IUI_CATEGORY:
	case IUI_COLOR:
	case IUI_COST:
	case IUI_CREATIONDATE:
	case IUI_CREATEDBY:
	case IUI_CUSTOMATTRIB:
	case IUI_DONEDATE:
	case IUI_DUEDATE:
	case IUI_EXTERNALID:
	case IUI_FILEREF:
	case IUI_FLAG:
	case IUI_ICON:
	case IUI_LASTMOD:
	case IUI_PERCENT:
	case IUI_PRIORITY:
	case IUI_RECURRENCE:
	case IUI_RISK:
	case IUI_STARTDATE:
	case IUI_STATUS:
	case IUI_SUBTASKDONE:
	case IUI_TAGS:
	case IUI_TASKNAME:
	case IUI_TIMEEST:
	case IUI_TIMESPENT:
	case IUI_VERSION:
		return TRUE;
	}
	
	// all else 
	return (nAttrib == IUI_ALL);
}

// External interface
BOOL CKanbanCtrlEx::WantSortUpdate(IUI_ATTRIBUTE nAttrib) const
{
	switch (nAttrib)
	{
	case IUI_NONE:
		return HasOption(KBCF_SORTSUBTASTASKSBELOWPARENTS);
	}

	// all else
	return WantEditUpdate(nAttrib);
}

BOOL CKanbanCtrlEx::RebuildData(const ITASKLISTBASE* pTasks, const CSet<IUI_ATTRIBUTE>& attrib)
{
	// Rebuild global attribute value lists
	m_mapAttributeValues.RemoveAll();
	m_aCustomAttribDefs.RemoveAll();

	UpdateGlobalAttributeValues(pTasks, attrib);

	// Rebuild data
	m_data.RemoveAll();

	return AddTaskToData(pTasks, pTasks->GetFirstTask(), 0, attrib, TRUE);
}

BOOL CKanbanCtrlEx::AddTaskToData(const ITASKLISTBASE* pTasks, HTASKITEM hTask, DWORD dwParentID, const CSet<IUI_ATTRIBUTE>& attrib, BOOL bAndSiblings)
{
	if (!hTask)
		return FALSE;

	// Not interested in references
	if (!pTasks->IsTaskReference(hTask))
	{
		DWORD dwTaskID = pTasks->GetTaskID(hTask);

		KANBANITEM* pKI = m_data.NewItem(dwTaskID, pTasks->GetTaskTitle(hTask));
		ASSERT(pKI);
	
		if (!pKI)
			return FALSE;

		pKI->bDone = pTasks->IsTaskDone(hTask);
		pKI->bGoodAsDone = pTasks->IsTaskGoodAsDone(hTask);
		pKI->bParent = pTasks->IsTaskParent(hTask);
		pKI->dwParentID = dwParentID;
		pKI->bLocked = pTasks->IsTaskLocked(hTask, true);
		pKI->bHasIcon = !Misc::IsEmpty(pTasks->GetTaskIcon(hTask));
		pKI->bFlag = (pTasks->IsTaskFlagged(hTask, false) ? TRUE : FALSE);
		pKI->nPosition = pTasks->GetTaskPosition(hTask);

		pKI->SetColor(pTasks->GetTaskTextColor(hTask));

		LPCWSTR szSubTaskDone = pTasks->GetTaskSubtaskCompletion(hTask);
		pKI->bSomeSubtaskDone = (!Misc::IsEmpty(szSubTaskDone) && (szSubTaskDone[0] != '0'));
	
		// Path is parent's path + parent's name
		if (dwParentID)
		{
			const KANBANITEM* pKIParent = m_data.GetItem(dwParentID);
			ASSERT(pKIParent);

			if (pKIParent->sPath.IsEmpty())
				pKI->sPath = pKIParent->sTitle;
			else
				pKI->sPath = pKIParent->sPath + '\\' + pKIParent->sTitle;

			pKI->nLevel = (pKIParent->nLevel + 1);
		}
		else
		{
			pKI->nLevel = 0;
		}

		// trackable attributes
		CStringArray aValues;

		if (GetTaskCategories(pTasks, hTask, aValues))
			pKI->SetTrackedAttributeValues(IUI_CATEGORY, aValues);

		if (GetTaskAllocTo(pTasks, hTask, aValues))
			pKI->SetTrackedAttributeValues(IUI_ALLOCTO, aValues);

		if (GetTaskTags(pTasks, hTask, aValues))
			pKI->SetTrackedAttributeValues(IUI_TAGS, aValues);
	
		pKI->SetTrackedAttributeValue(IUI_STATUS, pTasks->GetTaskStatus(hTask));
		pKI->SetTrackedAttributeValue(IUI_ALLOCBY, pTasks->GetTaskAllocatedBy(hTask));
		pKI->SetTrackedAttributeValue(IUI_VERSION, pTasks->GetTaskVersion(hTask));
		pKI->SetTrackedAttributeValue(IUI_PRIORITY, pTasks->GetTaskPriority(hTask, FALSE));
		pKI->SetTrackedAttributeValue(IUI_RISK, pTasks->GetTaskRisk(hTask, FALSE));

		// custom attributes
		int nCust = pTasks->GetCustomAttributeCount();

		while (nCust--)
		{
			CString sCustID(pTasks->GetCustomAttributeID(nCust));
			CString sCustValue(pTasks->GetTaskCustomAttributeData(hTask, sCustID, true));

			CStringArray aCustValues;
			Misc::Split(sCustValue, aCustValues, '+');

			pKI->SetTrackedAttributeValues(sCustID, aCustValues);

			// Add to global attribute values
			CKanbanValueMap* pValues = m_mapAttributeValues.GetAddMapping(sCustID);
			ASSERT(pValues);
			
			pValues->AddValues(aCustValues);
		}

		// Other display-only attributes
		UpdateItemDisplayAttributes(pKI, pTasks, hTask, attrib);

		// first child
		AddTaskToData(pTasks, pTasks->GetFirstTask(hTask), dwTaskID, attrib, TRUE);
	}

	// Siblings NON-RECURSIVELY
	if (bAndSiblings)
	{
		HTASKITEM hSibling = pTasks->GetNextTask(hTask);

		while (hSibling)
		{
			// FALSE == don't recurse on siblings
			AddTaskToData(pTasks, hSibling, dwParentID, attrib, FALSE);
			hSibling = pTasks->GetNextTask(hSibling);
		}
	}

	return TRUE;
}

BOOL CKanbanCtrlEx::UpdateData(const ITASKLISTBASE* pTasks, HTASKITEM hTask, const CSet<IUI_ATTRIBUTE>& attrib, BOOL bAndSiblings)
{
	if (hTask == NULL)
		return FALSE;

	// Not interested in references
	if (pTasks->IsTaskReference(hTask))
		return FALSE; 

	// handle task if not NULL (== root)
	BOOL bChange = FALSE;
	DWORD dwTaskID = pTasks->GetTaskID(hTask);

	if (dwTaskID)
	{
		// Can be a new task
		if (!HasKanbanItem(dwTaskID))
		{
			bChange = AddTaskToData(pTasks, hTask, pTasks->GetTaskParentID(hTask), attrib, FALSE);
		}
		else
		{
			KANBANITEM* pKI = NULL;
			GET_KI_RET(dwTaskID, pKI, FALSE);

			if (attrib.Has(IUI_TASKNAME))
				pKI->sTitle = pTasks->GetTaskTitle(hTask);
			
			if (attrib.Has(IUI_DONEDATE))
			{
				BOOL bDone = pTasks->IsTaskDone(hTask);
				BOOL bGoodAsDone = pTasks->IsTaskGoodAsDone(hTask);

				if ((pKI->bDone != bDone) || (pKI->bGoodAsDone != bGoodAsDone))
				{
					pKI->bDone = bDone;
					pKI->bGoodAsDone = bGoodAsDone;
				}
			}

			if (attrib.Has(IUI_SUBTASKDONE))
			{
				LPCWSTR szSubTaskDone = pTasks->GetTaskSubtaskCompletion(hTask);
				pKI->bSomeSubtaskDone = (!Misc::IsEmpty(szSubTaskDone) && (szSubTaskDone[0] != '0'));
			}

			if (attrib.Has(IUI_ICON))
				pKI->bHasIcon = !Misc::IsEmpty(pTasks->GetTaskIcon(hTask));

			if (attrib.Has(IUI_FLAG))
				pKI->bFlag = (pTasks->IsTaskFlagged(hTask, true) ? TRUE : FALSE);
			
			// Trackable attributes
			CStringArray aValues;

			if (attrib.Has(IUI_ALLOCTO))
			{
				GetTaskAllocTo(pTasks, hTask, aValues);
				bChange |= UpdateTrackableTaskAttribute(pKI, IUI_ALLOCTO, aValues);
			}

			if (attrib.Has(IUI_CATEGORY))
			{
				GetTaskCategories(pTasks, hTask, aValues);
				bChange |= UpdateTrackableTaskAttribute(pKI, IUI_CATEGORY, aValues);
			}

			if (attrib.Has(IUI_TAGS))
			{
				GetTaskTags(pTasks, hTask, aValues);
				bChange |= UpdateTrackableTaskAttribute(pKI, IUI_TAGS, aValues);
			}

			if (attrib.Has(IUI_ALLOCBY))
				bChange |= UpdateTrackableTaskAttribute(pKI, IUI_ALLOCBY, pTasks->GetTaskAllocatedBy(hTask));

			if (attrib.Has(IUI_STATUS))
				bChange |= UpdateTrackableTaskAttribute(pKI, IUI_STATUS, pTasks->GetTaskStatus(hTask));

			if (attrib.Has(IUI_VERSION))
				bChange |= UpdateTrackableTaskAttribute(pKI, IUI_VERSION, pTasks->GetTaskVersion(hTask));

			if (attrib.Has(IUI_PRIORITY))
				bChange |= UpdateTrackableTaskAttribute(pKI, IUI_PRIORITY, pTasks->GetTaskPriority(hTask, true));

			if (attrib.Has(IUI_RISK))
				bChange |= UpdateTrackableTaskAttribute(pKI, IUI_RISK, pTasks->GetTaskRisk(hTask, true));

			if (attrib.Has(IUI_CUSTOMATTRIB))
			{
				int nID = m_aCustomAttribDefs.GetSize();

				while (nID--)
				{
					KANBANCUSTOMATTRIBDEF& def = m_aCustomAttribDefs[nID];

					CString sValue(pTasks->GetTaskCustomAttributeData(hTask, def.sAttribID, true));
					CStringArray aValues;

					if (!sValue.IsEmpty())
					{
						if (Misc::Split(sValue, aValues, '+') > 1)
							def.bMultiValue = TRUE;
					}

					if (UpdateTrackableTaskAttribute(pKI, def.sAttribID, aValues))
					{
						// Add to global values
						CKanbanValueMap* pValues = m_mapAttributeValues.GetAddMapping(def.sAttribID);
						ASSERT(pValues);
						
						pValues->AddValues(aValues);
						bChange = TRUE;
					}
				}
			}

			// other display-only attributes
			UpdateItemDisplayAttributes(pKI, pTasks, hTask, attrib);
			
			// always update colour because it can change for so many reasons
			pKI->SetColor(pTasks->GetTaskTextColor(hTask));

			// Always update lock state
			pKI->bLocked = pTasks->IsTaskLocked(hTask, true);
		}
	}
		
	// children
	if (UpdateData(pTasks, pTasks->GetFirstTask(hTask), attrib, TRUE))
		bChange = TRUE;

	// handle siblings WITHOUT RECURSION
	if (bAndSiblings)
	{
		HTASKITEM hSibling = pTasks->GetNextTask(hTask);
		
		while (hSibling)
		{
			// FALSE == not siblings
			if (UpdateData(pTasks, hSibling, attrib, FALSE))
				bChange = TRUE;
			
			hSibling = pTasks->GetNextTask(hSibling);
		}
	}
	
	return bChange;
}

void CKanbanCtrlEx::UpdateItemDisplayAttributes(KANBANITEM* pKI, const ITASKLISTBASE* pTasks, HTASKITEM hTask, const CSet<IUI_ATTRIBUTE>& attrib)
{
	time64_t tDate = 0;
	
	if (attrib.Has(IUI_TIMEEST))
		pKI->dTimeEst = pTasks->GetTaskTimeEstimate(hTask, pKI->nTimeEstUnits, true);
	
	if (attrib.Has(IUI_TIMESPENT))
		pKI->dTimeSpent = pTasks->GetTaskTimeSpent(hTask, pKI->nTimeSpentUnits, true);
	
	if (attrib.Has(IUI_COST))
		pKI->dCost = pTasks->GetTaskCost(hTask, true);
	
	if (attrib.Has(IUI_CREATEDBY))
		pKI->sCreatedBy = pTasks->GetTaskCreatedBy(hTask);
	
	if (attrib.Has(IUI_CREATIONDATE))
		pKI->dtCreate = pTasks->GetTaskCreationDate(hTask);
	
	if (attrib.Has(IUI_DONEDATE) && pTasks->GetTaskDoneDate64(hTask, tDate))
		pKI->dtDone = CDateHelper::GetDate(tDate);
	
	if (attrib.Has(IUI_DUEDATE) && pTasks->GetTaskDueDate64(hTask, true, tDate))
		pKI->dtDue = CDateHelper::GetDate(tDate);
	
	if (attrib.Has(IUI_STARTDATE) && pTasks->GetTaskStartDate64(hTask, true, tDate))
		pKI->dtStart = CDateHelper::GetDate(tDate);
	
	if (attrib.Has(IUI_LASTMOD) && pTasks->GetTaskLastModified64(hTask, tDate))
		pKI->dtLastMod = CDateHelper::GetDate(tDate);
	
	if (attrib.Has(IUI_PERCENT))
		pKI->nPercent = pTasks->GetTaskPercentDone(hTask, true);
	
	if (attrib.Has(IUI_EXTERNALID))
		pKI->sExternalID = pTasks->GetTaskExternalID(hTask);
	
	if (attrib.Has(IUI_RECURRENCE))
		pKI->sRecurrence = pTasks->GetTaskAttribute(hTask, TDL_TASKRECURRENCE);

	if (attrib.Has(IUI_FILEREF) && pTasks->GetTaskFileLinkCount(hTask))
	{
		pKI->sFileRef = pTasks->GetTaskFileLink(hTask, 0);

		// Get the shortest meaningful bit because of space constraints
		if (FileMisc::IsPath(pKI->sFileRef))
			pKI->sFileRef = FileMisc::GetFileNameFromPath(pKI->sFileRef);
	}
}

BOOL CKanbanCtrlEx::UpdateGlobalAttributeValues(const ITASKLISTBASE* pTasks, const CSet<IUI_ATTRIBUTE>& attrib)
{
	BOOL bChange = FALSE;

	if (attrib.Has(IUI_STATUS))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_STATUS);
	
	if (attrib.Has(IUI_ALLOCTO))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_ALLOCTO);
	
	if (attrib.Has(IUI_CATEGORY))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_CATEGORY);
	
	if (attrib.Has(IUI_ALLOCBY))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_ALLOCBY);
	
	if (attrib.Has(IUI_TAGS))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_TAGS);
	
	if (attrib.Has(IUI_VERSION))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_VERSION);
	
	if (attrib.Has(IUI_PRIORITY))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_PRIORITY);
	
	if (attrib.Has(IUI_RISK))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_RISK);
	
	if (attrib.Has(IUI_CUSTOMATTRIB))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_CUSTOMATTRIB);
	
	return bChange;
}

BOOL CKanbanCtrlEx::UpdateGlobalAttributeValues(const ITASKLISTBASE* pTasks, IUI_ATTRIBUTE nAttribute)
{
	switch (nAttribute)
	{
	case IUI_PRIORITY:
	case IUI_RISK:
		{
			CString sAttribID(KANBANITEM::GetAttributeID(nAttribute));

			// create once only
			if (!m_mapAttributeValues.HasMapping(sAttribID))
			{
				CKanbanValueMap* pValues = m_mapAttributeValues.GetAddMapping(sAttribID);
				ASSERT(pValues);

				for (int nItem = 0; nItem <= 10; nItem++)
				{
					CString sValue(Misc::Format(nItem));
					pValues->SetAt(sValue, sValue);
				}
				
				// Add backlog item
				pValues->AddValue(EMPTY_STR);
			}
		}
		break;
		
	case IUI_STATUS:
	case IUI_ALLOCTO:
	case IUI_ALLOCBY:
	case IUI_CATEGORY:
	case IUI_VERSION:
	case IUI_TAGS:	
		{
			CString sXMLTag(GetXMLTag(nAttribute)); 
			CString sAttribID(KANBANITEM::GetAttributeID(nAttribute));

			CStringArray aNewValues;
			int nValue = pTasks->GetAttributeCount(sXMLTag);

			while (nValue--)
			{
				CString sValue(pTasks->GetAttributeItem(sXMLTag, nValue));

				if (!sValue.IsEmpty())
					aNewValues.Add(sValue);
			}

			return UpdateGlobalAttributeValues(sAttribID, aNewValues);
		}
		break;
		
	case IUI_CUSTOMATTRIB:
		{
			BOOL bChange = FALSE;
			int nCustom = pTasks->GetCustomAttributeCount();

			while (nCustom--)
			{
				// Save off each attribute ID
				if (pTasks->IsCustomAttributeEnabled(nCustom))
				{
					CString sAttribID(pTasks->GetCustomAttributeID(nCustom));
					CString sAttribName(pTasks->GetCustomAttributeLabel(nCustom));

					int nDef = m_aCustomAttribDefs.AddDefinition(sAttribID, sAttribName);

					// Add 'default' values to the map
					CKanbanValueMap* pDefValues = m_mapGlobalAttributeValues.GetAddMapping(sAttribID);
					ASSERT(pDefValues);

					pDefValues->RemoveAll();

					CString sListData = pTasks->GetCustomAttributeListData(nCustom);

					// 'Auto' list values follow 'default' list values
					//  separated by a TAB
					CString sDefData(sListData), sAutoData;
					Misc::Split(sDefData, sAutoData, '\t');

					CStringArray aDefValues;
					
					if (Misc::Split(sDefData, aDefValues, '\n'))
					{
						pDefValues->SetValues(aDefValues);

						if (aDefValues.GetSize() > 1)
							m_aCustomAttribDefs.SetMultiValue(nDef);
					}

					CStringArray aAutoValues;
					Misc::Split(sAutoData, aAutoValues, '\n');

					bChange |= UpdateGlobalAttributeValues(sAttribID, aAutoValues);
				}
			}

			return bChange;
		}
		break;
	}

	// all else
	return FALSE;
}

BOOL CKanbanCtrlEx::UpdateGlobalAttributeValues(LPCTSTR szAttribID, const CStringArray& aValues)
{
	CKanbanValueMap mapNewValues;
	mapNewValues.AddValues(aValues);

	// Add in Backlog value
	mapNewValues.AddValue(EMPTY_STR);

	// Merge in default values
	const CKanbanValueMap* pDefValues = m_mapGlobalAttributeValues.GetMapping(szAttribID);

	if (pDefValues)
		Misc::Append(*pDefValues, mapNewValues);

	// Check for changes
	CKanbanValueMap* pValues = m_mapAttributeValues.GetAddMapping(szAttribID);
	ASSERT(pValues);
	
	if (!Misc::MatchAll(mapNewValues, *pValues))
	{
		Misc::Copy(mapNewValues, *pValues);
		return TRUE;
	}

	// all else
	return FALSE;
}

int CKanbanCtrlEx::GetTaskTrackedAttributeValues(DWORD dwTaskID, CStringArray& aValues) const
{
	ASSERT(!m_sTrackAttribID.IsEmpty());

	const KANBANITEM* pKI = GetKanbanItem(dwTaskID);
	ASSERT(pKI);

	if (pKI)
		pKI->GetTrackedAttributeValues(m_sTrackAttribID, aValues);
	else
		aValues.RemoveAll();
	
	return aValues.GetSize();
}

int CKanbanCtrlEx::GetAttributeValues(IUI_ATTRIBUTE nAttrib, CStringArray& aValues) const
{
	CString sAttribID(KANBANITEM::GetAttributeID(nAttrib));

	const CKanbanValueMap* pValues = m_mapAttributeValues.GetMapping(sAttribID);
	aValues.SetSize(pValues->GetCount());

	if (pValues)
	{
		POSITION pos = pValues->GetStartPosition();
		int nItem = 0;

		while (pos)
			pValues->GetNextValue(pos, aValues[nItem++]);
	}

	return aValues.GetSize();
}

int CKanbanCtrlEx::GetAttributeValues(CKanbanAttributeValueMap& mapValues) const
{
	CString sAttribID;
	CKanbanValueMap* pValues = NULL;
	POSITION pos = m_mapAttributeValues.GetStartPosition();

	while (pos)
	{
		m_mapAttributeValues.GetNextAssoc(pos, sAttribID, pValues);
		ASSERT(pValues);

		CKanbanValueMap* pCopyValues = mapValues.GetAddMapping(sAttribID);
		ASSERT(pCopyValues);

		Misc::Copy(*pValues, *pCopyValues);
	}

	// Append default values
	pos = m_mapGlobalAttributeValues.GetStartPosition();

	while (pos)
	{
		m_mapGlobalAttributeValues.GetNextAssoc(pos, sAttribID, pValues);
		ASSERT(pValues);

		CKanbanValueMap* pCopyValues = mapValues.GetAddMapping(sAttribID);
		ASSERT(pCopyValues);

		Misc::Append(*pValues, *pCopyValues);
	}

	return mapValues.GetCount();
}

void CKanbanCtrlEx::LoadDefaultAttributeListValues(const IPreferences* pPrefs)
{
	m_mapGlobalAttributeValues.RemoveAll();

	LoadDefaultAttributeListValues(pPrefs, _T("ALLOCTO"),	_T("AllocToList"));
	LoadDefaultAttributeListValues(pPrefs, _T("ALLOCBY"),	_T("AllocByList"));
	LoadDefaultAttributeListValues(pPrefs, _T("STATUS"),	_T("StatusList"));
	LoadDefaultAttributeListValues(pPrefs, _T("CATEGORY"),	_T("CategoryList"));
	LoadDefaultAttributeListValues(pPrefs, _T("VERSION"),	_T("VersionList"));
	LoadDefaultAttributeListValues(pPrefs, _T("TAGS"),		_T("TagList"));

	if (m_nTrackAttribute != IUI_NONE)
		RebuildListCtrls(FALSE, FALSE);
}

void CKanbanCtrlEx::LoadDefaultAttributeListValues(const IPreferences* pPrefs, LPCTSTR szAttribID, LPCTSTR szSubKey)
{
	CKanbanValueMap* pMap = m_mapGlobalAttributeValues.GetAddMapping(szAttribID);
	ASSERT(pMap);

	CString sKey;
	sKey.Format(_T("Preferences\\%s"), szSubKey);

	int nCount = pPrefs->GetProfileInt(sKey, _T("ItemCount"), 0);
	
	// items
	for (int nItem = 0; nItem < nCount; nItem++)
	{
		CString sItemKey;
		sItemKey.Format(_T("Item%d"), nItem);

		CString sValue(pPrefs->GetProfileString(sKey, sItemKey));
		
		if (!sValue.IsEmpty())
			pMap->AddValue(sValue);
	}
}

CString CKanbanCtrlEx::GetXMLTag(IUI_ATTRIBUTE nAttrib)
{
	switch (nAttrib)
	{
	case IUI_ALLOCTO:	return TDL_TASKALLOCTO;
	case IUI_ALLOCBY:	return TDL_TASKALLOCBY;
	case IUI_STATUS:	return TDL_TASKSTATUS;
	case IUI_CATEGORY:	return TDL_TASKCATEGORY;
	case IUI_VERSION:	return TDL_TASKVERSION;
	case IUI_TAGS:		return TDL_TASKTAG;
		
	case IUI_CUSTOMATTRIB:
		ASSERT(0);
		break;
		
	default:
		ASSERT(0);
		break;
	}
	
	return EMPTY_STR;
}

BOOL CKanbanCtrlEx::UpdateTrackableTaskAttribute(KANBANITEM* pKI, IUI_ATTRIBUTE nAttrib, int nNewValue)
{
#ifdef _DEBUG
	switch (nAttrib)
	{
	case IUI_PRIORITY:
	case IUI_RISK:
		break;

	default:
		ASSERT(0);
		break;
	}
#endif

	CString sValue; // empty

	if (nNewValue >= 0)
		sValue = Misc::Format(nNewValue);
	
	// else empty
	return UpdateTrackableTaskAttribute(pKI, nAttrib, sValue);
}

BOOL CKanbanCtrlEx::IsTrackedAttributeMultiValue() const
{
	switch (m_nTrackAttribute)
	{
	case IUI_PRIORITY:
	case IUI_RISK:
	case IUI_ALLOCBY:
	case IUI_STATUS:
	case IUI_VERSION:
		return FALSE;

	case IUI_ALLOCTO:
	case IUI_CATEGORY:
	case IUI_TAGS:
		return TRUE;

	case IUI_CUSTOMATTRIB:
		{
			int nDef = m_aCustomAttribDefs.FindDefinition(m_sTrackAttribID);
			
			if (nDef != -1)
				return m_aCustomAttribDefs[nDef].bMultiValue;

		}
		break;
	}

	// all else
	ASSERT(0);
	return FALSE;
}

BOOL CKanbanCtrlEx::UpdateTrackableTaskAttribute(KANBANITEM* pKI, IUI_ATTRIBUTE nAttrib, const CString& sNewValue)
{
	CStringArray aNewValues;

	switch (nAttrib)
	{
	case IUI_PRIORITY:
	case IUI_RISK:
		if (!sNewValue.IsEmpty())
			aNewValues.Add(sNewValue);
		break;

	case IUI_ALLOCBY:
	case IUI_STATUS:
	case IUI_VERSION:
		aNewValues.Add(sNewValue);
		break;

	default:
		ASSERT(0);
		break;
	}
	
	return UpdateTrackableTaskAttribute(pKI, KANBANITEM::GetAttributeID(nAttrib), aNewValues);
}

BOOL CKanbanCtrlEx::UpdateTrackableTaskAttribute(KANBANITEM* pKI, IUI_ATTRIBUTE nAttrib, const CStringArray& aNewValues)
{
	switch (nAttrib)
	{
	case IUI_ALLOCTO:
	case IUI_CATEGORY:
	case IUI_TAGS:
		if (aNewValues.GetSize() == 0)
		{
			CStringArray aTemp;
			aTemp.Add(_T(""));

			return UpdateTrackableTaskAttribute(pKI, nAttrib, aTemp); // RECURSIVE CALL
		}
		break;

	default:
		ASSERT(0);
		return FALSE;
	}

	return UpdateTrackableTaskAttribute(pKI, KANBANITEM::GetAttributeID(nAttrib), aNewValues);
}

BOOL CKanbanCtrlEx::UpdateTrackableTaskAttribute(KANBANITEM* pKI, const CString& sAttribID, const CStringArray& aNewValues)
{
	// Check if we need to update listctrls or not
	if (!IsTracking(sAttribID) || (pKI->bParent && !HasOption(KBCF_SHOWPARENTTASKS)))
	{
		pKI->SetTrackedAttributeValues(sAttribID, aNewValues);
		return FALSE; // no effect on list items
	}

	// else
	BOOL bChange = FALSE;
	
	if (!pKI->AttributeValuesMatch(sAttribID, aNewValues))
	{
		CStringArray aCurValues;
		pKI->GetTrackedAttributeValues(sAttribID, aCurValues);
		
		// Remove any list item whose current value is not found in the new values
		int nVal = aCurValues.GetSize();
		
		// Special case: Item needs removing from backlog
		if (nVal == 0)
		{
			aCurValues.Add(_T(""));
			nVal++;
		}

		while (nVal--)
		{
			if (!Misc::Contains(aCurValues[nVal], aNewValues))
			{
				CKanbanListCtrlEx* pCurList = GetListCtrl(aCurValues[nVal]);
				ASSERT(pCurList);

				if (pCurList)
				{
					VERIFY(pCurList->DeleteTask(pKI->dwTaskID));
					bChange |= (pCurList->GetCount() == 0);
				}

				// Remove from list to speed up later searching
				aCurValues.RemoveAt(nVal);
			}
		}
		
		// Add any new items not in the current list
		nVal = aNewValues.GetSize();
		
		while (nVal--)
		{
			if (!Misc::Contains(aNewValues[nVal], aCurValues))
			{
				CKanbanListCtrlEx* pCurList = GetListCtrl(aNewValues[nVal]);
				
				if (pCurList)
					pCurList->AddTask(*pKI, FALSE);
				else
					bChange = TRUE; // needs new list ctrl
			}
		}
	
		// update values
		pKI->SetTrackedAttributeValues(sAttribID, aNewValues);
	}
	
	return bChange;
}

BOOL CKanbanCtrlEx::IsTracking(const CString& sAttribID) const
{
	return (m_sTrackAttribID.CompareNoCase(sAttribID) == 0);
}

BOOL CKanbanCtrlEx::WantShowColumn(LPCTSTR szValue, const CKanbanItemArrayMap& mapKIArray) const
{
	if (HasOption(KBCF_SHOWEMPTYCOLUMNS))
		return TRUE;

	if (HasOption(KBCF_ALWAYSSHOWBACKLOG) && Misc::IsEmpty(szValue))
		return TRUE;

	// else
	const CKanbanItemArray* pKIArr = mapKIArray.GetMapping(szValue);
		
	return (pKIArr && pKIArr->GetSize());
}

BOOL CKanbanCtrlEx::WantShowColumn(const CKanbanListCtrlEx* pList) const
{
	if (HasOption(KBCF_SHOWEMPTYCOLUMNS))
		return TRUE;

	if (HasOption(KBCF_ALWAYSSHOWBACKLOG) && pList->IsBacklog())
		return TRUE;

	return (pList->GetCount() > 0);
}

void CKanbanCtrlEx::RedrawListCtrls(BOOL bErase)
{
	m_aListCtrls.Redraw(bErase);
}

BOOL CKanbanCtrlEx::DeleteListCtrl(int nList)
{
	if ((nList < 0) || (nList >= m_aListCtrls.GetSize()))
	{
		ASSERT(0);
		return FALSE;
	}

	CKanbanListCtrlEx* pList = m_aListCtrls[nList];
	ASSERT(pList);

	if (pList == m_pSelectedList)
		m_pSelectedList = NULL;

	m_aListCtrls.RemoveAt(nList);

	return TRUE;
}

BOOL CKanbanCtrlEx::HasNonParentTasks(const CKanbanItemArray* pItems)
{
	ASSERT(pItems);

	int nItem = pItems->GetSize();

	while (nItem--)
	{
		if (!pItems->GetAt(nItem)->bParent)
			return TRUE;
	}

	// else all parents
	return FALSE;
}

int CKanbanCtrlEx::RemoveOldDynamicListCtrls(const CKanbanItemArrayMap& mapKIArray)
{
	if (!UsingDynamicColumns())
	{
		ASSERT(0);
		return 0;
	}

	// remove any lists whose values are no longer used
	// or which Optionally have no items 
	const CKanbanValueMap* pGlobals = m_mapAttributeValues.GetMapping(m_sTrackAttribID);
	int nList = m_aListCtrls.GetSize(), nNumRemoved = 0;
	
	while (nList--)
	{
		CKanbanListCtrlEx* pList = m_aListCtrls[nList];
		ASSERT(pList && !pList->HasMultipleValues());
		
		if ((pGlobals == NULL) || !WantShowColumn(pList))
		{
			DeleteListCtrl(nList);
			nNumRemoved++;
		}
		else
		{
			CString sAttribValueID(pList->GetAttributeValueID());
			
			if (!Misc::HasKey(*pGlobals, sAttribValueID) || 
				!WantShowColumn(sAttribValueID, mapKIArray))
			{
				DeleteListCtrl(nList);
				nNumRemoved++;
			}
		}
	}

	return nNumRemoved;
}

int CKanbanCtrlEx::AddMissingDynamicListCtrls(const CKanbanItemArrayMap& mapKIArray)
{
	if (!UsingDynamicColumns())
	{
		ASSERT(0);
		return 0;
	}
	
	// Add any new status lists not yet existing
	const CKanbanValueMap* pGlobals = m_mapAttributeValues.GetMapping(m_sTrackAttribID);
	int nNumAdded = 0;

	if (pGlobals)
	{
		POSITION pos = pGlobals->GetStartPosition();
		
		while (pos)
		{
			CString sAttribValueID, sAttribValue;
			pGlobals->GetNextAssoc(pos, sAttribValueID, sAttribValue);
			
			CKanbanListCtrlEx* pList = GetListCtrl(sAttribValueID);
			
			if ((pList == NULL) && WantShowColumn(sAttribValueID, mapKIArray))
			{
				KANBANCOLUMN colDef;
				
				colDef.sAttribID = m_sTrackAttribID;
				colDef.sTitle = sAttribValue;
				colDef.aAttribValues.Add(sAttribValue);
				//colDef.crBackground = KBCOLORS[m_nNextColor++ % NUM_KBCOLORS];
				
				VERIFY (AddNewListCtrl(colDef) != NULL);
				nNumAdded++;
			}
		}

		ASSERT(!HasOption(KBCF_SHOWEMPTYCOLUMNS) || 
				(m_nTrackAttribute == IUI_CUSTOMATTRIB) ||
				(m_aListCtrls.GetSize() == pGlobals->GetCount()));
	}

	return nNumAdded;
}

void CKanbanCtrlEx::RebuildDynamicListCtrls(const CKanbanItemArrayMap& mapKIArray)
{
	if (!UsingDynamicColumns())
	{
		ASSERT(0);
		return;
	}
	
	BOOL bNeedResize = RemoveOldDynamicListCtrls(mapKIArray);
	bNeedResize |= AddMissingDynamicListCtrls(mapKIArray);

	// If no columns created, create empty Backlog column
	bNeedResize |= CheckAddBacklogListCtrl();
	
	// (Re)sort
	m_aListCtrls.Sort();

	RebuildHeaderColumns(); // always

	if (bNeedResize)
		Resize();
}

void CKanbanCtrlEx::RebuildFixedListCtrls(const CKanbanItemArrayMap& mapKIArray)
{
	if (!UsingFixedColumns())
	{
		ASSERT(0);
		return;
	}

	if (m_aListCtrls.GetSize() == 0) // first time only
	{
		for (int nDef = 0; nDef < m_aColumnDefs.GetSize(); nDef++)
		{
			const KANBANCOLUMN& colDef = m_aColumnDefs[nDef];
			VERIFY(AddNewListCtrl(colDef) != NULL);
		}

		RebuildHeaderColumns(); // always
		Resize(); // always
	}
}

BOOL CKanbanCtrlEx::CheckAddBacklogListCtrl()
{
	if (m_aListCtrls.GetSize() == 0) 
	{
		KANBANCOLUMN colDef;
		
		colDef.sAttribID = m_sTrackAttribID;
		colDef.aAttribValues.Add(_T(""));
		
		VERIFY (AddNewListCtrl(colDef) != NULL);
		return TRUE;
	}

	return FALSE;
}

void CKanbanCtrlEx::RebuildListCtrls(BOOL bRebuildData, BOOL bTaskUpdate)
{
	if (m_sTrackAttribID.IsEmpty())
	{
		ASSERT(m_nTrackAttribute == IUI_NONE);
		return;
	}

	CHoldRedraw gr(*this, NCR_PAINT | NCR_ERASEBKGND);

	DWORD dwSelTaskID = GetSelectedTaskID();
	
	CKanbanItemArrayMap mapKIArray;
	m_data.BuildTempItemMaps(m_sTrackAttribID, mapKIArray);

	if (UsingDynamicColumns())
		RebuildDynamicListCtrls(mapKIArray);
	else
		RebuildFixedListCtrls(mapKIArray);

	// Rebuild the list data for each list (which can be empty)
	if (bRebuildData)
	{
		RebuildListCtrlData(mapKIArray);
	}
	else if (UsingDynamicColumns())
	{
		// If not rebuilding the data (which will remove lists
		// which are empty as consequence of not showing parent
		// tasks) we made need to remove such lists.
		RemoveOldDynamicListCtrls(mapKIArray);
	}

	Resize();
		
	// We only need to restore selection if not doing a task update
	// because the app takes care of that
	if (!bTaskUpdate && dwSelTaskID && !SelectTask(dwSelTaskID))
	{
		if (!m_pSelectedList || !Misc::HasT(m_aListCtrls, m_pSelectedList))
		{
			// Find the first list with some items
			m_pSelectedList = m_aListCtrls.GetFirstNonEmpty();

			// No list has items?
			if (!m_pSelectedList)
				m_pSelectedList = m_aListCtrls[0];
		}
	}
}

void CKanbanCtrlEx::RebuildHeaderColumns()
{
	int nNumColumns = GetVisibleColumnCount();

	while (nNumColumns < m_header.GetItemCount())
	{
		m_header.DeleteItem(0);
	}

	while (nNumColumns > m_header.GetItemCount())
	{
		m_header.AppendItem(1);
	}

	for (int nCol = 0; nCol < nNumColumns; nCol++)
	{
		const CKanbanListCtrlEx* pList = m_aListCtrls[nCol];

		CEnString sTitle(pList->ColumnDefinition().sTitle);

		if (sTitle.IsEmpty())
			sTitle.LoadString(IDS_BACKLOG);

		CString sFormat;
		sFormat.Format(_T("%s (%d)"), sTitle, pList->GetCount());

		m_header.SetItemText(nCol, sFormat);
		m_header.SetItemData(nCol, (DWORD)pList);
	}
}

void CKanbanCtrlEx::RebuildListCtrlData(const CKanbanItemArrayMap& mapKIArray)
{
	BOOL bShowParents = HasOption(KBCF_SHOWPARENTTASKS);
	int nList = m_aListCtrls.GetSize();
	
	while (nList--)
	{
		CKanbanListCtrlEx* pList = m_aListCtrls[nList];
		ASSERT(pList);
		
		RebuildListContents(pList, mapKIArray, bShowParents);
		
		// The list can still end up empty if parent tasks are 
		// omitted in Dynamic columns so we recheck and delete if required
		if (UsingDynamicColumns())
		{
			if (!bShowParents && !WantShowColumn(pList))
			{
				DeleteListCtrl(nList);
			}
		}
	}
		
	// Lists can still end up empty if there were 
	// only unwanted parents
	CheckAddBacklogListCtrl();

	// Resort
	Sort(m_nSortBy, m_bSortAscending);
}

void CKanbanCtrlEx::FixupSelectedList()
{
	ASSERT(m_aListCtrls.GetSize());

	// Make sure selected list is valid
	if (!m_pSelectedList || !Misc::HasT(m_aListCtrls, m_pSelectedList))
	{
		// Find the first list with some items
		m_pSelectedList = m_aListCtrls.GetFirstNonEmpty();

		// No list has items?
		if (!m_pSelectedList)
			m_pSelectedList = m_aListCtrls[0];
	}

	FixupListFocus();
}

void CKanbanCtrlEx::FixupListFocus()
{
	if (IsWindowVisible() && HasFocus() && (GetFocus() != m_pSelectedList))
	{
		CAutoFlag af(m_bSettingListFocus, TRUE);

		m_pSelectedList->SetFocus();
		m_pSelectedList->Invalidate(TRUE);
	}
}

IUI_ATTRIBUTE CKanbanCtrlEx::GetTrackedAttribute(CString& sCustomAttrib) const
{
	if (m_nTrackAttribute == IUI_CUSTOMATTRIB)
		sCustomAttrib = m_sTrackAttribID;
	else
		sCustomAttrib.Empty();

	return m_nTrackAttribute;
}

BOOL CKanbanCtrlEx::TrackAttribute(IUI_ATTRIBUTE nAttrib, const CString& sCustomAttribID, 
								 const CKanbanColumnArray& aColumnDefs)
{
	// validate input and check for changes
	BOOL bChange = (nAttrib != m_nTrackAttribute);

	switch (nAttrib)
	{
	case IUI_STATUS:
	case IUI_ALLOCTO:
	case IUI_ALLOCBY:
	case IUI_CATEGORY:
	case IUI_VERSION:
	case IUI_PRIORITY:
	case IUI_RISK:
	case IUI_TAGS:
		break;
		
	case IUI_CUSTOMATTRIB:
		if (sCustomAttribID.IsEmpty())
			return FALSE;

		if (!bChange)
			bChange = (sCustomAttribID != m_sTrackAttribID);
		break;

	default:
		return FALSE;
	}

	// Check if only display attributes have changed
	if (!bChange)
	{
		if (UsingFixedColumns())
		{
			if (m_aColumnDefs.MatchesAll(aColumnDefs))
			{
				return TRUE;
			}
			else if (m_aColumnDefs.MatchesAll(aColumnDefs, FALSE))
			{
				int nCol = aColumnDefs.GetSize();
				ASSERT(nCol == m_aListCtrls.GetSize());

				while (nCol--)
				{
					const KANBANCOLUMN& colDef = aColumnDefs[nCol];
					CKanbanListCtrlEx* pList = m_aListCtrls[nCol];
					ASSERT(pList);

					if (pList)
					{
						pList->SetBackgroundColor(colDef.crBackground);
						//pList->SetExcessColor(colDef.crExcess);
						//pList->SetMaximumTaskCount(colDef.nMaxTaskCount);
					}
				}
				return TRUE;
			}
		}
		else if (!aColumnDefs.GetSize()) // not switching to fixed columns
		{
			return TRUE;
		}
	}

	m_aColumnDefs.Copy(aColumnDefs);

	// update state
	m_nTrackAttribute = nAttrib;

	switch (nAttrib)
	{
	case IUI_STATUS:
	case IUI_ALLOCTO:
	case IUI_ALLOCBY:
	case IUI_CATEGORY:
	case IUI_VERSION:
	case IUI_PRIORITY:
	case IUI_RISK:
	case IUI_TAGS:
		m_sTrackAttribID = KANBANITEM::GetAttributeID(nAttrib);
		break;
		
	case IUI_CUSTOMATTRIB:
		m_sTrackAttribID = sCustomAttribID;
		break;
	}

	// delete all lists and start over
	CHoldRedraw gr(*this, NCR_PAINT | NCR_ERASEBKGND);

	m_pSelectedList = NULL;
	m_aListCtrls.RemoveAll();

	RebuildListCtrls(TRUE, TRUE);
	Resize();

	return TRUE;
}

CKanbanListCtrlEx* CKanbanCtrlEx::AddNewListCtrl(const KANBANCOLUMN& colDef)
{
	CKanbanListCtrlEx* pList = new CKanbanListCtrlEx(m_data, colDef, m_fonts, m_aPriorityColors, m_aDisplayAttrib);
	ASSERT(pList);

	if (pList)
	{
		pList->SetTextColorIsBackground(HasOption(KBCF_TASKTEXTCOLORISBKGND));
		pList->SetStrikeThruDoneTasks(HasOption(KBCF_STRIKETHRUDONETASKS));
		pList->SetColorTaskBarByPriority(HasOption(KBCF_COLORBARBYPRIORITY));
		pList->SetShowTaskColorAsBar(HasOption(KBCF_SHOWTASKCOLORASBAR));
		pList->SetShowCompletionCheckboxes(HasOption(KBCF_SHOWCOMPLETIONCHECKBOXES));
		pList->SetIndentSubtasks(HasOption(KBCF_INDENTSUBTASKS));

		if (pList->Create(IDC_LISTCTRL, this))
		{
			m_aListCtrls.Add(pList);
		}
		else
		{
			delete pList;
			pList = NULL;
		}
	}
	
	return pList;
}

BOOL CKanbanCtrlEx::RebuildListContents(CKanbanListCtrlEx* pList, const CKanbanItemArrayMap& mapKIArray, BOOL bShowParents)
{
	ASSERT(pList && pList->GetSafeHwnd());

	if (!pList || !pList->GetSafeHwnd())
		return FALSE;

	DWORD dwSelID = pList->GetSelectedTaskID();

	pList->SetRedraw(FALSE);
	pList->DeleteAllItems();

	CStringArray aValueIDs;
	int nNumVals = pList->GetAttributeValueIDs(aValueIDs);

	for (int nVal = 0; nVal < nNumVals; nVal++)
	{
		const CKanbanItemArray* pKIArr = mapKIArray.GetMapping(aValueIDs[nVal]);
		
		if (pKIArr)
		{
			ASSERT(pKIArr->GetSize());
			
			for (int nKI = 0; nKI < pKIArr->GetSize(); nKI++)
			{
				const KANBANITEM* pKI = pKIArr->GetAt(nKI);
				ASSERT(pKI);
				
				if (!pKI->bParent || bShowParents)
				{
					BOOL bSelected = (dwSelID == pKI->dwTaskID);

					VERIFY(pList->AddTask(*pKI, bSelected) != NULL);
				}
			}
		}
	}
	
	pList->RefreshItemLineHeights();
	pList->SetRedraw(TRUE);

	return TRUE;
}

void CKanbanCtrlEx::BuildTaskIDMap(const ITASKLISTBASE* pTasks, HTASKITEM hTask, CDWordSet& mapIDs, BOOL bAndSiblings)
{
	if (hTask == NULL)
		return;
	
	mapIDs.Add(pTasks->GetTaskID(hTask));
	
	// children
	BuildTaskIDMap(pTasks, pTasks->GetFirstTask(hTask), mapIDs, TRUE);
	
	// handle siblings WITHOUT RECURSION
	if (bAndSiblings)
	{
		HTASKITEM hSibling = pTasks->GetNextTask(hTask);
		
		while (hSibling)
		{
			// FALSE == not siblings
			BuildTaskIDMap(pTasks, hSibling, mapIDs, FALSE);
			hSibling = pTasks->GetNextTask(hSibling);
		}
	}
}

void CKanbanCtrlEx::RemoveDeletedTasks(const ITASKLISTBASE* pTasks)
{
	CDWordSet mapIDs;
	BuildTaskIDMap(pTasks, pTasks->GetFirstTask(NULL), mapIDs, TRUE);

	m_aListCtrls.RemoveDeletedTasks(mapIDs);
	m_data.RemoveDeletedItems(mapIDs);
}

KANBANITEM* CKanbanCtrlEx::GetKanbanItem(DWORD dwTaskID) const
{
	return m_data.GetItem(dwTaskID);
}

BOOL CKanbanCtrlEx::HasKanbanItem(DWORD dwTaskID) const
{
	return m_data.HasItem(dwTaskID);
}

CKanbanListCtrlEx* CKanbanCtrlEx::LocateTask(DWORD dwTaskID, HTREEITEM& hti, BOOL bForward) const
{
	// First try selected list
	if (m_pSelectedList)
	{
		hti = m_pSelectedList->FindTask(dwTaskID);

		if (hti)
			return m_pSelectedList;
	}

	// try any other list in the specified direction
	const CKanbanListCtrlEx* pList = GetNextListCtrl(m_pSelectedList, bForward, TRUE);

	if (!pList)
		return NULL;

	const CKanbanListCtrlEx* pStartList = pList;

	do
	{
		hti = pList->FindTask(dwTaskID);

		if (hti)
			return const_cast<CKanbanListCtrlEx*>(pList);

		// else
		pList = GetNextListCtrl(pList, bForward, TRUE);
	}
	while (pList != pStartList);

	return NULL;
}

CKanbanListCtrlEx* CKanbanCtrlEx::GetListCtrl(const CString& sAttribValue) const
{
	return m_aListCtrls.Get(sAttribValue);
}

CKanbanListCtrlEx* CKanbanCtrlEx::GetListCtrl(HWND hwnd) const
{
	return m_aListCtrls.Get(hwnd);
}

void CKanbanCtrlEx::SetDisplayAttributes(const CKanbanAttributeArray& aAttrib)
{
	if (!Misc::MatchAllT(m_aDisplayAttrib, aAttrib, FALSE))
	{
		m_aDisplayAttrib.Copy(aAttrib);
		m_aListCtrls.OnDisplayAttributeChanged();

		// Update list attribute label visibility
		if (m_aDisplayAttrib.GetSize())
			Resize();
	}
}

int CKanbanCtrlEx::GetDisplayAttributes(CKanbanAttributeArray& aAttrib) const
{
	aAttrib.Copy(m_aDisplayAttrib);
	return aAttrib.GetSize();
}

void CKanbanCtrlEx::SetOption(DWORD dwOption, BOOL bSet)
{
	if (dwOption)
	{
		DWORD dwPrev = m_dwOptions;

		if (bSet)
			m_dwOptions |= dwOption;
		else
			m_dwOptions &= ~dwOption;

		// specific handling
		if (m_dwOptions != dwPrev)
		{
			switch (dwOption)
			{
			case KBCF_SHOWPARENTTASKS:
				RebuildListCtrls(TRUE, FALSE);
				break;

			case KBCF_SORTSUBTASTASKSBELOWPARENTS:
				if (m_nSortBy != IUI_NONE)
					Sort(m_nSortBy, m_bSortAscending);
				break;

			case KBCF_SHOWEMPTYCOLUMNS:
			case KBCF_ALWAYSSHOWBACKLOG:
				if (m_aListCtrls.GetSize())
					RebuildListCtrls(FALSE, FALSE);
				break;

			case KBCF_TASKTEXTCOLORISBKGND:
				m_aListCtrls.SetTextColorIsBackground(bSet);
				break;

			case KBCF_COLORBARBYPRIORITY:
				m_aListCtrls.SetColorTaskBarByPriority(bSet);
				break;

			case KBCF_STRIKETHRUDONETASKS:
				m_aListCtrls.SetStrikeThruDoneTasks(bSet);
				break;

			case KBCF_SHOWTASKCOLORASBAR:
				m_aListCtrls.SetShowTaskColorAsBar(bSet);
				break;

			case KBCF_SHOWCOMPLETIONCHECKBOXES:
				m_aListCtrls.SetShowCompletionCheckboxes(bSet);
				break;

			case KBCF_INDENTSUBTASKS:
				m_aListCtrls.SetIndentSubtasks(bSet);
				break;
			}
		}
	}
}

void CKanbanCtrlEx::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);

	Resize(CRect(0, 0, cx, cy));
}

BOOL CKanbanCtrlEx::OnEraseBkgnd(CDC* pDC)
{
	if (m_aListCtrls.GetSize())
	{
		CDialogHelper::ExcludeChild(&m_header, pDC);

		// Clip out the list controls
		m_aListCtrls.Exclude(pDC);
		
		// fill the client with gray to create borders
		CRect rClient;
		GetClientRect(rClient);
		
		pDC->FillSolidRect(rClient, GetSysColor(COLOR_3DSHADOW));
	}
	
	return TRUE;
}

void CKanbanCtrlEx::OnSetFocus(CWnd* pOldWnd)
{
	CWnd::OnSetFocus(pOldWnd);

	FixupListFocus();
	ScrollToSelectedTask();
}

int CKanbanCtrlEx::GetVisibleListCtrlCount() const
{
	if (UsingDynamicColumns() || HasOption(KBCF_SHOWEMPTYCOLUMNS))
		return m_aListCtrls.GetSize();

	// Fixed columns
	BOOL bShowBacklog = HasOption(KBCF_ALWAYSSHOWBACKLOG);

	return m_aListCtrls.GetVisibleCount(bShowBacklog);
}

void CKanbanCtrlEx::Resize(const CRect& rect)
{
	int nNumVisibleLists = GetVisibleListCtrlCount();

	if (nNumVisibleLists)
	{
		ASSERT(m_header.GetItemCount() == nNumVisibleLists);

		BOOL bHideEmpty = !HasOption(KBCF_SHOWEMPTYCOLUMNS);
		BOOL bShowBacklog = HasOption(KBCF_ALWAYSSHOWBACKLOG);

		// Reduce client by a pixel to create a border
		CRect rAvail(rect);
		rAvail.DeflateRect(1, 1);

		CRect rHeader(rAvail);
		rHeader.bottom = (rHeader.top + GraphicsMisc::ScaleByDPIFactor(24));
	
		m_header.MoveWindow(rHeader);
		rAvail.top = rHeader.bottom;
		
		CString sStatus;
		int nListWidth = (rAvail.Width() / nNumVisibleLists);

		// Check whether the lists are wide enough to show attribute labels
		KBC_ATTRIBLABELS nLabelVis = GetListAttributeLabelVisibility(nListWidth);

		// Also update tab order as we go
		int nNumLists = m_aListCtrls.GetSize();
		CWnd* pPrev = NULL;

		for (int nList = 0, nVis = 0; nList < nNumLists; nList++)
		{
			CKanbanListCtrlEx* pList = m_aListCtrls[nList];
			ASSERT(pList && pList->GetSafeHwnd());
			
			// If we find an empty column, it can only be with 
			// Fixed columns because we only hide columns rather
			// than delete them
			if (UsingFixedColumns())
			{
				if (!WantShowColumn(pList))
				{
					pList->ShowWindow(SW_HIDE);
					pList->EnableWindow(FALSE);
					continue;
				}

				// else
				pList->ShowWindow(SW_SHOW);
				pList->EnableWindow(TRUE);
			}

			CRect rList(rAvail);
			rList.left = (rAvail.left + (nVis * (nListWidth + 1))); // +1 to create a gap

			// Last column takes up rest of width
			if (nVis == (nNumVisibleLists - 1))
				rList.right = rAvail.right;
			else
				rList.right = (rList.left + nListWidth);

			pList->SetWindowPos(pPrev, rList.left, rList.top, rList.Width(), rList.Height(), 0);
			pList->SetAttributeLabelVisibility(nLabelVis);

			m_header.SetItemWidth(nVis, rList.Width() + 1); // +1 to allow for column gap

			pPrev = pList;
			nVis++;
		}
	}
}

float CKanbanCtrlEx::GetAverageListCharWidth()
{
	return m_aListCtrls.GetAverageCharWidth();
}

BOOL CKanbanCtrlEx::CanFitAttributeLabels(int nAvailWidth, float fAveCharWidth, KBC_ATTRIBLABELS nLabelVis) const
{
	switch (nLabelVis)
	{
	case KBCAL_NONE:
		return TRUE;

	case KBCAL_LONG:
	case KBCAL_SHORT:
		{
			int nAtt = m_aDisplayAttrib.GetSize();
			CUIntArray aLabelLen;

			aLabelLen.SetSize(nAtt);

			while (nAtt--)
			{
				IUI_ATTRIBUTE nAttribID = m_aDisplayAttrib[nAtt];
				CString sLabel = CKanbanListCtrlEx::FormatAttribute(nAttribID, _T(""), nLabelVis);

				aLabelLen[nAtt] = sLabel.GetLength();

				if ((int)(aLabelLen[nAtt] * fAveCharWidth) > nAvailWidth)
					return FALSE;
			}

			// Look for the first 'Label: Value' item which exceeds the list width
			POSITION pos = m_data.GetStartPosition();

			while (pos)
			{
				KANBANITEM* pKI = NULL;
				DWORD dwTaskID = 0;

				m_data.GetNextAssoc(pos, dwTaskID, pKI);

				int nAtt = m_aDisplayAttrib.GetSize();

				while (nAtt--)
				{
					IUI_ATTRIBUTE nAttribID = m_aDisplayAttrib[nAtt];

					// Exclude 'File Link' and 'Parent' because these will 
					// almost always push things over the limit
					if ((nAttribID != IUI_FILEREF) && (nAttribID != IUI_PARENT))
					{
						int nValueLen = pKI->GetAttributeDisplayValue(nAttribID).GetLength();

						if ((int)((aLabelLen[nAtt] + nValueLen) * fAveCharWidth) > nAvailWidth)
							return FALSE;
					}
				}
			}

			// else
			return TRUE;
		}
		break;
	}

	// all else
	ASSERT(0);
	return FALSE;
}

KBC_ATTRIBLABELS CKanbanCtrlEx::GetListAttributeLabelVisibility(int nListWidth)
{
	if (!m_aDisplayAttrib.GetSize() || !m_aListCtrls.GetSize())
		return KBCAL_NONE;

	// Calculate the available width for attributes
	int nAvailWidth = m_aListCtrls[0]->CalcAvailableAttributeWidth(nListWidth);

	// Calculate the fixed attribute label lengths and check if any
	// of them exceed the list width
	float fAveCharWidth = GetAverageListCharWidth();
	KBC_ATTRIBLABELS nLabelVis[2] = { KBCAL_LONG, KBCAL_SHORT };

	for (int nPass = 0; nPass < 2; nPass++)
	{
		if (CanFitAttributeLabels(nAvailWidth, fAveCharWidth, nLabelVis[nPass]))
			return nLabelVis[nPass];
	}

	return KBCAL_NONE;
}

void CKanbanCtrlEx::Resize()
{
	CRect rClient;
	GetClientRect(rClient);

	Resize(rClient);
}

void CKanbanCtrlEx::Sort(IUI_ATTRIBUTE nBy, BOOL bAscending)
{
	// if the sort attribute equals the track attribute then
	// tasks are already sorted into separate columns so we  
	// sort by title instead
	if ((nBy != IUI_NONE) && (nBy == m_nTrackAttribute))
		nBy = IUI_TASKNAME;
	
	m_nSortBy = nBy;

	BOOL bSubtasksBelowParent = HasOption(KBCF_SORTSUBTASTASKSBELOWPARENTS);
	
	if ((m_nSortBy != IUI_NONE) || bSubtasksBelowParent)
	{
		ASSERT((m_nSortBy == IUI_NONE) || (bAscending != -1));
		m_bSortAscending = bAscending;

		// do the sort
 		CHoldRedraw hr(*this);

		m_aListCtrls.SortItems(m_nSortBy, m_bSortAscending, bSubtasksBelowParent);
	}
}

void CKanbanCtrlEx::SetReadOnly(bool bReadOnly) 
{ 
	m_bReadOnly = bReadOnly; 
}

BOOL CKanbanCtrlEx::GetLabelEditRect(LPRECT pEdit)
{
	if (!m_pSelectedList || !m_pSelectedList->GetLabelEditRect(pEdit))
	{
		ASSERT(0);
		return FALSE;
	}

	// else convert from list to 'our' coords
	m_pSelectedList->ClientToScreen(pEdit);
	ScreenToClient(pEdit);

	return TRUE;
}

void CKanbanCtrlEx::SetPriorityColors(const CDWordArray& aColors)
{
	if (!Misc::MatchAll(m_aPriorityColors, aColors))
	{
		m_aPriorityColors.Copy(aColors);

		// Redraw the lists if coloring by priority
		if (GetSafeHwnd() && HasOption(KBCF_COLORBARBYPRIORITY))
			RedrawListCtrls(TRUE);
	}
}

void CKanbanCtrlEx::ScrollToSelectedTask()
{
	CKanbanListCtrlEx* pList = GetSelListCtrl();

	if (pList)
		pList->ScrollToSelection();
}

bool CKanbanCtrlEx::PrepareNewTask(ITaskList* pTask) const
{
	ITASKLISTBASE* pTasks = GetITLInterface<ITASKLISTBASE>(pTask, IID_TASKLISTBASE);

	if (pTasks == NULL)
	{
		ASSERT(0);
		return false;
	}

	HTASKITEM hNewTask = pTasks->GetFirstTask();
	ASSERT(hNewTask);

	const CKanbanListCtrlEx* pList = GetSelListCtrl();
	CString sValue;

	CRect rListCtrl;
	pList->GetWindowRect(rListCtrl);

	if (!GetListCtrlAttributeValue(pList, rListCtrl.CenterPoint(), sValue))
		return false;

	switch (m_nTrackAttribute)
	{
	case IUI_STATUS:
		pTasks->SetTaskStatus(hNewTask, sValue);
		break;

	case IUI_ALLOCTO:
		pTasks->AddTaskAllocatedTo(hNewTask, sValue);
		break;

	case IUI_ALLOCBY:
		pTasks->SetTaskAllocatedBy(hNewTask, sValue);
		break;

	case IUI_CATEGORY:
		pTasks->AddTaskCategory(hNewTask, sValue);
		break;

	case IUI_PRIORITY:
		pTasks->SetTaskPriority(hNewTask, _ttoi(sValue));
		break;

	case IUI_RISK:
		pTasks->SetTaskRisk(hNewTask, _ttoi(sValue));
		break;

	case IUI_VERSION:
		pTasks->SetTaskVersion(hNewTask, sValue);
		break;

	case IUI_TAGS:
		pTasks->AddTaskTag(hNewTask, sValue);
		break;

	case IUI_CUSTOMATTRIB:
		ASSERT(!m_sTrackAttribID.IsEmpty());
		pTasks->SetTaskCustomAttributeData(hNewTask, m_sTrackAttribID, sValue);
		break;
	}

	return true;
}

DWORD CKanbanCtrlEx::HitTestTask(const CPoint& ptScreen) const
{
	return m_aListCtrls.HitTestTask(ptScreen);
}

DWORD CKanbanCtrlEx::GetNextTask(DWORD dwTaskID, IUI_APPCOMMAND nCmd) const
{
	BOOL bForward = ((nCmd == IUI_GETPREVTASK) || (nCmd == IUI_GETPREVTOPLEVELTASK));

	HTREEITEM hti = NULL;
	const CKanbanListCtrlEx* pList = LocateTask(dwTaskID, hti, bForward);
	
	if (!pList || (UsingFixedColumns() && !pList->IsWindowVisible()))
	{
		return 0L;
	}
	else if (hti == NULL)
	{
		ASSERT(0);
		return 0L;
	}

	DWORD dwNextID(dwTaskID);

	switch (nCmd)
	{
	case IUI_GETNEXTTASK:
		dwNextID = pList->GetTaskID(pList->GetNextSiblingItem(hti));
		break;

	case IUI_GETNEXTTOPLEVELTASK:
		{
			const CKanbanListCtrlEx* pNext = GetNextListCtrl(pList, TRUE, TRUE);
			
			if (pNext)
				dwNextID = pNext->GetTaskID(pNext->TCH().GetFirstItem());
		}
		break;

	case IUI_GETPREVTASK:
		dwNextID = pList->GetTaskID(pList->GetPrevSiblingItem(hti));
		break;

	case IUI_GETPREVTOPLEVELTASK:
		{
			const CKanbanListCtrlEx* pPrev = GetNextListCtrl(pList, FALSE, TRUE);
			
			if (pPrev)
				dwNextID = pPrev->GetTaskID(pPrev->TCH().GetFirstItem());
		}
		break;

	default:
		ASSERT(0);
	}

	return dwNextID;
}

const CKanbanListCtrlEx* CKanbanCtrlEx::GetNextListCtrl(const CKanbanListCtrlEx* pList, BOOL bNext, BOOL bExcludeEmpty) const
{
	return m_aListCtrls.GetNext(pList, bNext, bExcludeEmpty, UsingFixedColumns());
}

CKanbanListCtrlEx* CKanbanCtrlEx::GetNextListCtrl(const CKanbanListCtrlEx* pList, BOOL bNext, BOOL bExcludeEmpty)
{
	return m_aListCtrls.GetNext(pList, bNext, bExcludeEmpty, UsingFixedColumns());
}

CKanbanListCtrlEx* CKanbanCtrlEx::HitTestListCtrl(const CPoint& ptScreen, BOOL* pbHeader) const
{
	return m_aListCtrls.HitTest(ptScreen, pbHeader);
}

BOOL CKanbanCtrlEx::IsDragging() const
{
	ASSERT(!m_bReadOnly);

	return (!m_bReadOnly && (::GetCapture() == GetSafeHwnd()));
}

BOOL CKanbanCtrlEx::NotifyParentAttibuteChange(DWORD dwTaskID)
{
	ASSERT(!m_bReadOnly);
	ASSERT(dwTaskID);

	return GetParent()->SendMessage(WM_KBC_VALUECHANGE, (WPARAM)GetSafeHwnd(), dwTaskID);
}

void CKanbanCtrlEx::NotifyParentSelectionChange()
{
	ASSERT(!m_bSelectTasks);

	GetParent()->SendMessage(WM_KBC_SELECTIONCHANGE, GetSelectedTaskID(), 0);
}

// external version
BOOL CKanbanCtrlEx::CancelOperation()
{
	if (IsDragging())
	{
		ReleaseCapture();
		return TRUE;
	}
	
	// else 
	return FALSE;
}

BOOL CKanbanCtrlEx::SelectListCtrl(CKanbanListCtrlEx* pList, BOOL bNotifyParent)
{
	if (pList)
	{
		if (pList == m_pSelectedList)
		{
			// Make sure header is refreshed
			m_pSelectedList->SetSelected(TRUE);
			return TRUE;
		}

		CKanbanListCtrlEx* pPrevSelList = m_pSelectedList;
		m_pSelectedList = pList;

		FixupListFocus();

		if (pList->GetCount() > 0)
		{
			ClearOtherListSelections(m_pSelectedList);

			if (m_pSelectedList->GetSelectedCount())
				m_pSelectedList->ScrollToSelection();

			if (bNotifyParent)
				NotifyParentSelectionChange();
		}
		else
		{
			pPrevSelList->SetSelected(FALSE);
			m_pSelectedList->SetSelected(TRUE);
		}

		m_header.Invalidate(TRUE);

		return TRUE;
	}

	return FALSE;
}

BOOL CKanbanCtrlEx::IsSelectedListCtrl(HWND hWnd) const
{
	return (m_pSelectedList && (m_pSelectedList->GetSafeHwnd() == hWnd));
}

void CKanbanCtrlEx::OnListItemSelChange(NMHDR* pNMHDR, LRESULT* pResult)
{
	// only interested in selection changes from the selected list 
	// and occurring outside of a drag'n'drop or a call to 'SelectTasks', because the 'actual' 
	// selected task IDs will not change during a drag'n'drop
	if (!m_bSelectTasks && !IsDragging())
	{
		CKanbanListCtrlEx* pList = m_aListCtrls.Get(pNMHDR->hwndFrom);

		if (pList && (pList != m_pSelectedList))
		{
			CAutoFlag af(m_bSelectTasks, TRUE);
			SelectListCtrl(pList, FALSE);
		}

		NotifyParentSelectionChange();
	}
#ifdef _DEBUG
	else
	{
		int breakpoint = 0;
	}
#endif
}

void CKanbanCtrlEx::OnListEditLabel(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = TRUE; // cancel our edit

	NMTVDISPINFO* pNMTV = (NMTVDISPINFO*)pNMHDR;
	ASSERT(pNMTV->item.lParam);

	GetParent()->SendMessage(WM_KBC_EDITTASKTITLE, pNMTV->item.lParam);
}

void CKanbanCtrlEx::OnHeaderCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMCUSTOMDRAW* pNMCD = (NMCUSTOMDRAW*)pNMHDR;
	*pResult = CDRF_DODEFAULT;

	HWND hwndHdr = pNMCD->hdr.hwndFrom;
	ASSERT(hwndHdr == m_header);
	
	switch (pNMCD->dwDrawStage)
	{
	case CDDS_PREPAINT:
		// Handle RTL text column headers and selected column
		*pResult = CDRF_NOTIFYITEMDRAW;
		break;
		
	case CDDS_ITEMPREPAINT:
		if (GraphicsMisc::GetRTLDrawTextFlags(hwndHdr) == DT_RTLREADING)
		{
			*pResult = CDRF_NOTIFYPOSTPAINT;
		}
		else if (m_pSelectedList)
		{
			// Show the text of the selected column in bold
			if (pNMCD->lItemlParam == (LPARAM)m_pSelectedList)
				::SelectObject(pNMCD->hdc, m_fonts.GetHFont(GMFS_BOLD));
			else
				::SelectObject(pNMCD->hdc, m_fonts.GetHFont());
			
			*pResult = CDRF_NEWFONT;
		}
		break;
		
	case CDDS_ITEMPOSTPAINT:
		{
			ASSERT(GraphicsMisc::GetRTLDrawTextFlags(hwndHdr) == DT_RTLREADING);

			CRect rItem(pNMCD->rc);
			rItem.DeflateRect(3, 0);

			CDC* pDC = CDC::FromHandle(pNMCD->hdc);
			pDC->SetBkMode(TRANSPARENT);

			// Show the text of the selected column in bold
			HGDIOBJ hPrev = NULL;

			if (pNMCD->lItemlParam == (LPARAM)m_pSelectedList)
				hPrev = pDC->SelectObject(m_fonts.GetHFont(GMFS_BOLD));
			else
				hPrev = pDC->SelectObject(m_fonts.GetHFont());
			
			UINT nFlags = (DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | GraphicsMisc::GetRTLDrawTextFlags(hwndHdr));
			pDC->DrawText(m_header.GetItemText(pNMCD->dwItemSpec), rItem, nFlags);

			pDC->SelectObject(hPrev);
			
			*pResult = CDRF_SKIPDEFAULT;
		}
		break;
	}
}

void CKanbanCtrlEx::ClearOtherListSelections(const CKanbanListCtrlEx* pIgnore)
{
	m_aListCtrls.ClearOtherSelections(pIgnore);
}

void CKanbanCtrlEx::OnBeginDragListItem(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (!m_bReadOnly)
	{
		ASSERT(!IsDragging());
		ASSERT(pNMHDR->idFrom == IDC_LISTCTRL);
		
		NMLISTVIEW* pNMLV = (NMLISTVIEW*)pNMHDR;
		ASSERT(pNMLV->iItem != -1);
		
		CKanbanListCtrlEx* pList = (CKanbanListCtrlEx*)CWnd::FromHandle(pNMHDR->hwndFrom);

		if (!pList->SelectionHasLockedTasks())
		{
			DWORD dwDragID = pList->GetSelectedTaskID();

			if (dwDragID)
			{
				// If the 'drag-from' list is not currently selected
				// we select it and then reset the selection to the
				// items we have just copied
				if (pList != m_pSelectedList)
				{
					VERIFY(pList->SelectTask(dwDragID));
					SelectListCtrl(pList);
				}

				SetCapture();
			}
		}
	}
	
	*pResult = 0;
}

BOOL CKanbanCtrlEx::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	CPoint ptCursor(GetMessagePos());
	DWORD dwTaskID = HitTestTask(ptCursor);

	if (m_data.IsLocked(dwTaskID))
		return GraphicsMisc::SetAppCursor(_T("Locked"), _T("Resources\\Cursors"));

	// else
	return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

void CKanbanCtrlEx::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (IsDragging())
	{
		// get the list under the mouse
		ClientToScreen(&point);

		CKanbanListCtrlEx* pDestList = HitTestListCtrl(point);
		CKanbanListCtrlEx* pSrcList = m_pSelectedList;

		if (CanDrag(pSrcList, pDestList))
		{
			CString sDestAttribValue;
			
			if (GetListCtrlAttributeValue(pDestList, point, sDestAttribValue))
			{
				DWORD dwDragID = pSrcList->GetSelectedTaskID();
				ASSERT(dwDragID);

				BOOL bChange = EndDragItem(pSrcList, dwDragID, pDestList, sDestAttribValue);

				if (!WantShowColumn(pSrcList) && UsingDynamicColumns())
				{
					int nList = Misc::FindT(m_aListCtrls, pSrcList);
					ASSERT(nList != -1);

					m_aListCtrls.RemoveAt(nList);
				}

				Resize();

				if (bChange)
				{
					// Resort before fixing up selection
					BOOL bSubtasksBelowParent = HasOption(KBCF_SORTSUBTASTASKSBELOWPARENTS);
					
					if ((m_nSortBy != IUI_NONE) || bSubtasksBelowParent)
						pDestList->Sort(m_nSortBy, m_bSortAscending, bSubtasksBelowParent);

					SelectListCtrl(pDestList, FALSE);
					SelectTask(dwDragID); 

					NotifyParentSelectionChange();
					NotifyParentAttibuteChange(dwDragID);
				}
			}
		}
	}

	// always
	ReleaseCapture();

	CWnd::OnLButtonUp(nFlags, point);
}

BOOL CKanbanCtrlEx::CanDrag(const CKanbanListCtrlEx* pSrcList, const CKanbanListCtrlEx* pDestList) const
{
	// Can only copy MULTI-VALUE attributes
	if (Misc::ModKeysArePressed(MKS_CTRL) && !IsTrackedAttributeMultiValue())
		return FALSE;

	return CKanbanListCtrlEx::CanDrag(pSrcList, pDestList);
}

BOOL CKanbanCtrlEx::EndDragItem(CKanbanListCtrlEx* pSrcList, DWORD dwTaskID, 
								CKanbanListCtrlEx* pDestList, const CString& sDestAttribValue)
{
	ASSERT(CanDrag(pSrcList, pDestList));
	ASSERT(pSrcList->FindTask(dwTaskID) != NULL);

	KANBANITEM* pKI = GetKanbanItem(dwTaskID);

	if (!pKI)
	{
		ASSERT(0);
		return FALSE;
	}

	BOOL bSrcIsBacklog = pSrcList->IsBacklog();
	BOOL bDestIsBacklog = pDestList->IsBacklog();
	BOOL bCopy = (!bSrcIsBacklog && 
					Misc::ModKeysArePressed(MKS_CTRL) &&
					IsTrackedAttributeMultiValue());

	// Remove from the source list(s) if moving
	if (bSrcIsBacklog)
	{
		VERIFY(pSrcList->DeleteTask(dwTaskID));
	}
	else if (!bCopy) // move
	{
		// Remove all values
		pKI->RemoveAllTrackedAttributeValues(m_sTrackAttribID);

		// Remove from all src lists
		m_aListCtrls.DeleteTaskFromOthers(dwTaskID, pDestList);
	}
	else if (bDestIsBacklog) // and 'copy'
	{
		// Just remove the source list's value(s)
		CStringArray aSrcValues;
		int nVal = pSrcList->GetAttributeValues(aSrcValues);

		while (nVal--)
			pKI->RemoveTrackedAttributeValue(m_sTrackAttribID, aSrcValues[nVal]);

		VERIFY(pSrcList->DeleteTask(dwTaskID));
	}

	// Append to the destination list
	if (bDestIsBacklog)
	{
		if (!pKI->HasTrackedAttributeValues(m_sTrackAttribID))
			pDestList->AddTask(*pKI, TRUE);
	}
	else
	{
		pKI->AddTrackedAttributeValue(m_sTrackAttribID, sDestAttribValue);

		if (pDestList->FindTask(dwTaskID) == NULL)
			pDestList->AddTask(*pKI, TRUE);
	}

	return TRUE;
}

BOOL CKanbanCtrlEx::GetListCtrlAttributeValue(const CKanbanListCtrlEx* pDestList, const CPoint& ptScreen, CString& sValue) const
{
	CStringArray aListValues;
	int nNumValues = pDestList->GetAttributeValues(aListValues);

	switch (nNumValues)
	{
	case 0: // Backlog
		sValue.Empty();
		return TRUE;
		
	case 1:
		sValue = aListValues[0];
		return TRUE;
	}

	// List has multiple values -> show popup menu
	CMenu menu;
	VERIFY (menu.CreatePopupMenu());

	for (int nVal = 0; nVal < nNumValues; nVal++)
	{
		menu.AppendMenu(MF_STRING, (nVal + 1), aListValues[nVal]);
	}

	UINT nValID = menu.TrackPopupMenu((TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD), 
										ptScreen.x, ptScreen.y, CWnd::FromHandle(*pDestList));

	if (nValID > 0)
	{
		sValue = aListValues[nValID - 1];
		return TRUE;
	}

	// user cancelled
	return FALSE;
}

void CKanbanCtrlEx::OnMouseMove(UINT nFlags, CPoint point)
{
	if (IsDragging())
	{
		ASSERT(!m_bReadOnly);
		
		// get the list under the mouse
		ClientToScreen(&point);
		const CKanbanListCtrlEx* pDestList = HitTestListCtrl(point);

		BOOL bValidDest = CanDrag(m_pSelectedList, pDestList);
		BOOL bCopy = Misc::ModKeysArePressed(MKS_CTRL);

		GraphicsMisc::SetDragDropCursor(bValidDest ? (bCopy ? GMOC_COPY : GMOC_MOVE) : GMOC_NO);
	}
	
	CWnd::OnMouseMove(nFlags, point);
}

BOOL CKanbanCtrlEx::SaveToImage(CBitmap& bmImage)
{
	return m_aListCtrls.SaveToImage(bmImage);
}

int CKanbanCtrlEx::CalcRequiredColumnWidthForImage() const
{
	return m_aListCtrls.CalcRequiredColumnWidthForImage();
}

BOOL CKanbanCtrlEx::CanSaveToImage() const
{
	return m_aListCtrls.CanSaveToImage();
}

LRESULT CKanbanCtrlEx::OnSetFont(WPARAM wp, LPARAM lp)
{
	m_fonts.Initialise((HFONT)wp, FALSE);
	m_aListCtrls.OnSetFont((HFONT)wp);
	m_header.SendMessage(WM_SETFONT, wp, lp);

	return 0L;
}

LRESULT CKanbanCtrlEx::OnListCheckChange(WPARAM /*wp*/, LPARAM lp)
{
	ASSERT(!m_bReadOnly);

	DWORD dwTaskID = lp;
	const KANBANITEM* pKI = m_data.GetItem(dwTaskID);
	ASSERT(pKI);

	if (pKI)
	{
		LRESULT lr = GetParent()->SendMessage(WM_KBC_COMPLETIONCHANGE, (WPARAM)GetSafeHwnd(), !pKI->IsDone(FALSE));

		if (lr && m_data.HasItem(dwTaskID))
			PostMessage(WM_KCM_SELECTTASK, 0, dwTaskID);

		return lr;
	}

	// else
	return 0L;
}

LRESULT CKanbanCtrlEx::OnSelectTask(WPARAM /*wp*/, LPARAM lp)
{
	return SelectTask(lp);
}

LRESULT CKanbanCtrlEx::OnListGetTaskIcon(WPARAM wp, LPARAM lp)
{
	return GetParent()->SendMessage(WM_KBC_GETTASKICON, wp, lp);
}

void CKanbanCtrlEx::OnListSetFocus(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	// Reverse focus changes outside of our own doing
	if (!m_bSettingListFocus)
	{
		FixupListFocus();
	}
}