// PluginHelpers.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "Toolbars.h"

////////////////////////////////////////////////////////////////////////////////////////////////

#using <system.dll>

using namespace Abstractspoon::Tdl::PluginHelpers;

////////////////////////////////////////////////////////////////////////////////////////////////

void Toolbars::FixupButtonSizes(ToolStrip^ toolbar)
{
	auto imageSize = toolbar->ImageScalingSize;
	int numItems = toolbar->Items->Count;

	for (int i = 0; i < numItems; i++)
	{
		auto item = toolbar->Items[i];

		if (Toolbars::IsButton(item))
		{
			auto button = Toolbars::AsButton(item);

			// From 'Shared\EnToolBar.cpp'
			int xPadding = (imageSize.Width + 7 - button->Size.Width);
			int yPadding = (imageSize.Height + 7 - button->Size.Height);

			button->Padding = Padding(xPadding / 2, yPadding / 2, xPadding - xPadding / 2, yPadding - yPadding / 2);
		}
		else if (Toolbars::IsDropDown(item))
		{
			auto drop = Toolbars::AsDropDown(item);

			// From 'Shared\EnToolBar.cpp'
			int yPadding = (imageSize.Height + 7 - drop->Size.Height);

			drop->Padding = Padding(0, (yPadding / 2), 0, (yPadding - (yPadding / 2)));
		}
		else if (Toolbars::IsSeparator(item))
		{
			auto sep = Toolbars::AsSeparator(item);

			sep->AutoSize = false;
			sep->Height = (imageSize.Height + 7);
		}
	}
}

bool Toolbars::IsButton(ToolStripItem^ item)
{
	return (nullptr != AsButton(item));
}

bool Toolbars::IsDropDown(ToolStripItem^ item)
{
	return (nullptr != AsDropDown(item));
}

bool Toolbars::IsMenu(ToolStripItem^ item)
{
	return (nullptr != AsMenu(item));
}

bool Toolbars::IsSeparator(ToolStripItem^ item)
{
	return (nullptr != AsSeparator(item));
}

ToolStripButton^ Toolbars::AsButton(ToolStripItem^ item)
{
	return dynamic_cast<ToolStripButton^>(item);
}

ToolStripDropDownItem^ Toolbars::AsDropDown(ToolStripItem^ item)
{
	return dynamic_cast<ToolStripDropDownItem^>(item);
}

ToolStripMenuItem^ Toolbars::AsMenu(ToolStripItem^ item)
{
	return dynamic_cast<ToolStripMenuItem^>(item);
}

ToolStripSeparator^ Toolbars::AsSeparator(ToolStripItem^ item)
{
	return dynamic_cast<ToolStripSeparator^>(item);
}

void Toolbars::Sort(ToolStripItemCollection^ items)
{
	auto oAList = gcnew System::Collections::ArrayList(items);
	oAList->Sort(gcnew ToolStripItemComparer());
	items->Clear();

	for (int i = 0; i < oAList->Count; i++)
	{
		items->Add(dynamic_cast<ToolStripItem^>(oAList[i]));
	}
}

int Toolbars::ToolStripItemComparer::Compare(System::Object^ obj1, System::Object^ obj2)
{
	auto oItem1 = dynamic_cast<ToolStripItem^>(obj1);
	auto oItem2 = dynamic_cast<ToolStripItem^>(obj2);

	return String::Compare(oItem1->Text, oItem2->Text, true);
}

////////////////////////////////////////////////////////////////////////////////////////////////

BaseToolbarRenderer::BaseToolbarRenderer() 
	: 
	m_DrawRowDividers(true),
	m_DrawLeftBorder(false), 
	m_DrawTopBorder(false), 
	m_DrawRightBorder(false), 
	m_DrawBottomBorder(false)
{

}

void BaseToolbarRenderer::EnableDrawRowDividers(bool enable)
{
	m_DrawRowDividers = enable;
}

void BaseToolbarRenderer::EnableDrawBorders(ToolStrip^ toolbar, bool left, bool top, bool right, bool bottom)
{
	const int BORDERSIZE = 2;
	int leftChange = 0, topChange = 0, rightChange = 0, botChange = 0;

	if (m_DrawLeftBorder != left)
	{
		leftChange = (left ? BORDERSIZE : -BORDERSIZE);
		m_DrawLeftBorder = left;
	}

	if (m_DrawTopBorder != top)
	{
		topChange = (top ? BORDERSIZE : -BORDERSIZE);
		m_DrawTopBorder = top;
	}
	
	if (m_DrawRightBorder != right)
	{
		rightChange = (right ? BORDERSIZE : -BORDERSIZE);
		m_DrawRightBorder = right;
	}

	if (m_DrawBottomBorder != bottom)
	{
		botChange = (bottom ? BORDERSIZE : -BORDERSIZE);
		m_DrawBottomBorder = bottom;
	}

	auto padding = toolbar->Padding;
	toolbar->Padding = Padding(padding.Left + leftChange, 
							  padding.Top + topChange, 
							  padding.Right + rightChange, 
							  padding.Bottom + botChange);
}

void BaseToolbarRenderer::OnRenderToolStripBorder(ToolStripRenderEventArgs^ e)
{
	// Eat this to prevent the default drawing of a right hand border
	//int breakpoint = 0;
}

void BaseToolbarRenderer::OnRenderToolStripBackground(ToolStripRenderEventArgs^ e)
{
	ToolStripRenderer::OnRenderToolStripBackground(e);

	auto toolbar = e->ToolStrip;

	if (m_DrawRowDividers)
	{
		int numItems = toolbar->Items->Count;

		if (numItems > 0)
		{
			auto prevItemRect = Drawing::Rectangle::Empty;
			int rowTop = toolbar->Top, rowBottom = 0;
			bool firstRow = true;

			for (int i = 0; i < numItems; i++)
			{
				auto item = toolbar->Items[i];

				if (!Toolbars::IsSeparator(item) && item->Visible)
				{
					if (prevItemRect.IsEmpty)
					{
						prevItemRect = item->Bounds;
					}
					else
					{
						auto itemRect = item->Bounds;

						if (itemRect.Top > prevItemRect.Top)
						{
							rowBottom = ((itemRect.Top + prevItemRect.Bottom) / 2);

							auto rowRect = gcnew Drawing::Rectangle(toolbar->Left, rowTop, toolbar->Width, (rowBottom - rowTop));
							DrawRowBackground(e->Graphics, rowRect, firstRow, false);
							
							prevItemRect = itemRect;
							rowTop = rowBottom + 1;
							firstRow = false;
						}
					}
				}
			}

			// Last row
			auto rowRect = gcnew Drawing::Rectangle(toolbar->Left, rowTop, toolbar->Width, (toolbar->Bottom - rowTop));
			DrawRowBackground(e->Graphics, rowRect, firstRow, true);
		}
	}

	if (m_DrawLeftBorder)
	{
// 		e->Graphics->DrawLine(SystemPens::ButtonHighlight, toolbar->ClientRectangle.Left, toolbar->ClientRectangle.Top, toolbar->ClientRectangle.Right, toolbar->ClientRectangle.Top);
// 		e->Graphics->DrawLine(SystemPens::ButtonShadow, toolbar->ClientRectangle.Left, toolbar->ClientRectangle.Bottom, toolbar->ClientRectangle.Right, toolbar->ClientRectangle.Bottom);
	}

	if (m_DrawTopBorder)
	{
// 		e->Graphics->DrawLine(SystemPens::ButtonHighlight, toolbar->ClientRectangle.Left, toolbar->ClientRectangle.Top, toolbar->ClientRectangle.Right, toolbar->ClientRectangle.Top);
// 		e->Graphics->DrawLine(SystemPens::ButtonShadow, toolbar->ClientRectangle.Left, toolbar->ClientRectangle.Top + 1, toolbar->ClientRectangle.Right, toolbar->ClientRectangle.Top + 1);
	}

	if (m_DrawRightBorder)
	{
// 		e->Graphics->DrawLine(SystemPens::ButtonHighlight, toolbar->ClientRectangle.Left, toolbar->ClientRectangle.Top, toolbar->ClientRectangle.Right, toolbar->ClientRectangle.Top);
// 		e->Graphics->DrawLine(SystemPens::ButtonShadow, toolbar->ClientRectangle.Left, toolbar->ClientRectangle.Bottom, toolbar->ClientRectangle.Right, toolbar->ClientRectangle.Bottom);
	}

	if (m_DrawBottomBorder)
	{
		e->Graphics->DrawLine(SystemPens::ButtonHighlight, toolbar->ClientRectangle.Left, toolbar->ClientRectangle.Bottom - 3, toolbar->ClientRectangle.Right, toolbar->ClientRectangle.Bottom - 3);
		e->Graphics->DrawLine(SystemPens::ButtonShadow, toolbar->ClientRectangle.Left, toolbar->ClientRectangle.Bottom - 2, toolbar->ClientRectangle.Right, toolbar->ClientRectangle.Bottom - 2);
	}
}

void BaseToolbarRenderer::DrawRowBackground(Drawing::Graphics^ g, Drawing::Rectangle^ rowRect, bool firstRow, bool lastRow)
{
	DrawRowDivider(g, rowRect, firstRow, lastRow);
}

void BaseToolbarRenderer::DrawRowDivider(Drawing::Graphics^ g, Drawing::Rectangle^ rowRect, bool firstRow, bool lastRow)
{
	// Draw highlight line at top if not first row
	if (!firstRow)
		g->DrawLine(SystemPens::ButtonHighlight, rowRect->Left, rowRect->Top, rowRect->Right, rowRect->Top);
	
	// Draw shadow line at bottom if not last row
	if (!lastRow)
		g->DrawLine(SystemPens::ButtonShadow, rowRect->Left, rowRect->Bottom, rowRect->Right, rowRect->Bottom);
}

////////////////////////////////////////////////////////////////////////////////////////////////
