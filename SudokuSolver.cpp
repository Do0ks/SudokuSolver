#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cmath>
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "Comctl32.lib")

#include <tchar.h>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>
#include <thread>

// Control IDs
#define IDC_COMBO_SIZE       2001
#define IDC_GENERATE_BUTTON  2002
#define IDC_CLEAR_BUTTON     2003
#define IDC_SOLVE_BUTTON     2004

// Global instance and control handles
HINSTANCE g_hInst = NULL;
HWND hComboSize = NULL;
HWND hButtonGenerate = NULL;
HWND hButtonClear = NULL;
HWND hButtonSolve = NULL;

// Sudoku grid controls
std::vector<std::vector<HWND>> gCells;
std::vector<HWND> gGridLines;

// Board parameters
int gBoardSize = 9;
int gSubgridRows = 3;
int gSubgridCols = 3;

// Fixed puzzle area inside the window
const int PUZZLE_START_X = 10;
const int PUZZLE_START_Y = 50;
const int PUZZLE_WIDTH = 500;
const int PUZZLE_HEIGHT = 500;

// Spacing/line constants
const int CELL_SPACING = 8;
const int LINE_THICKNESS = 4;

// Solver flags
bool gIsSolving = false;
bool gStopRequested = false;

// Global font for cells
static HFONT gCellFont = NULL;

// -----------------------------------------------------------------------------
// Subclass Procedure: Auto-advance, vertical centering, and Tab/Shift+Tab navigation
// If Shift+Tab is pressed, after moving focus to the previous control, the control's content is selected.
// -----------------------------------------------------------------------------
LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    switch (uMsg)
    {
    case WM_CHAR:
    {
        if (wParam == VK_RETURN)
            return 0;
        LRESULT lr = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (wParam != VK_BACK)
        {
            int len = GetWindowTextLength(hWnd);
            if (len >= 1)
            {
                HWND hParent = GetParent(hWnd);
                HWND hNext = GetNextDlgTabItem(hParent, hWnd, FALSE);
                if (hNext)
                    SetFocus(hNext);
            }
        }
        return lr;
    }
    case WM_KEYDOWN:
    {
        if (wParam == VK_TAB)
        {
            bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            HWND hParent = GetParent(hWnd);
            HWND hNext = GetNextDlgTabItem(hParent, hWnd, shiftPressed);
            if (hNext)
            {
                SetFocus(hNext);
                if (shiftPressed)
                {
                    SendMessage(hNext, EM_SETSEL, 0, -1);
                }
                return 0;
            }
        }
        break;
    }
    case WM_SETFONT:
    {
        LRESULT lr = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        HFONT hFont = (HFONT)wParam;
        if (!hFont)
            return lr;
        // Measure the font to compute vertical centering.
        HDC hdc = GetDC(hWnd);
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        TEXTMETRIC tm;
        GetTextMetrics(hdc, &tm);
        SelectObject(hdc, hOldFont);
        ReleaseDC(hWnd, hdc);
        RECT rcClient;
        GetClientRect(hWnd, &rcClient);
        int boxHeight = rcClient.bottom - rcClient.top;
        int textHeight = tm.tmHeight + tm.tmExternalLeading;
        int offset = (boxHeight - textHeight) / 2;
        if (offset < 0)
            offset = 0;
        RECT rcInset = rcClient;
        rcInset.top += offset;
        SendMessage(hWnd, EM_SETRECT, 0, (LPARAM)&rcInset);
        return lr;
    }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// -----------------------------------------------------------------------------
// Helper: Convert typed char to integer [1..boardSize], or 0 if blank
// -----------------------------------------------------------------------------
int charToValue(TCHAR c, int boardSize)
{
    if (c == _T('\0') || c == _T('0') || c == _T('.') || c == _T(' '))
        return 0;
    if (c >= _T('1') && c <= _T('9'))
    {
        int val = c - _T('0');
        return (val <= boardSize) ? val : 0;
    }
    if (c >= _T('A') && c <= _T('Z'))
    {
        int val = 10 + (c - _T('A'));
        return (val <= boardSize) ? val : 0;
    }
    return 0;
}

TCHAR valueToChar(int val, int boardSize)
{
    if (val == 0)
        return _T('\0');
    if (val <= 9)
        return static_cast<TCHAR>(_T('0') + val);
    return static_cast<TCHAR>(_T('A') + (val - 10));
}

// -----------------------------------------------------------------------------
// Subgrid Dimensions Helper
// -----------------------------------------------------------------------------
void SetSubgridDimensions(int boardSize)
{
    switch (boardSize)
    {
    case 4:  gSubgridRows = 2; gSubgridCols = 2; break;
    case 6:  gSubgridRows = 2; gSubgridCols = 3; break;
    case 9:  gSubgridRows = 3; gSubgridCols = 3; break;
    case 16: gSubgridRows = 4; gSubgridCols = 4; break;
    default:
    {
        int s = (int)std::sqrt(boardSize);
        if (s * s == boardSize)
        {
            gSubgridRows = s;
            gSubgridCols = s;
        }
        else
        {
            gSubgridRows = 1;
            gSubgridCols = boardSize;
        }
    }
    break;
    }
}

// -----------------------------------------------------------------------------
// Create/Destroy the Sudoku Grid
// -----------------------------------------------------------------------------
void DestroyGrid(HWND)
{
    if (gCellFont)
    {
        DeleteObject(gCellFont);
        gCellFont = NULL;
    }
    for (auto& row : gCells)
    {
        for (HWND cell : row)
        {
            if (cell)
                DestroyWindow(cell);
        }
    }
    gCells.clear();
    for (HWND line : gGridLines)
    {
        if (line)
            DestroyWindow(line);
    }
    gGridLines.clear();
}

HFONT CreateScaledFont(int cellSize)
{
    // Use half the cell size in points for the font.
    int approximatePointSize = cellSize / 2;
    if (approximatePointSize < 8)
        approximatePointSize = 8;
    HDC hdc = GetDC(NULL);
    int logPixelsY = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);
    int fontHeight = -MulDiv(approximatePointSize, logPixelsY, 72);
    HFONT hFont = CreateFont(
        fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        _T("Arial")
    );
    return hFont;
}

void CreateGridLines(HWND hParent, int cellSize, int offsetX, int offsetY)
{
    int gridWidth = gBoardSize * (cellSize + CELL_SPACING) - CELL_SPACING;
    int gridHeight = gBoardSize * (cellSize + CELL_SPACING) - CELL_SPACING;
    // Vertical lines
    int numV = gBoardSize / gSubgridCols;
    for (int k = 1; k < numV; k++)
    {
        int x = offsetX + k * gSubgridCols * (cellSize + CELL_SPACING)
            - (CELL_SPACING / 2) - (LINE_THICKNESS / 2);
        HWND hLine = CreateWindowEx(0, _T("STATIC"), NULL,
            WS_CHILD | WS_VISIBLE | SS_BLACKRECT,
            x, offsetY, LINE_THICKNESS, gridHeight,
            hParent, NULL, g_hInst, NULL);
        gGridLines.push_back(hLine);
    }
    // Horizontal lines
    int numH = gBoardSize / gSubgridRows;
    for (int k = 1; k < numH; k++)
    {
        int y = offsetY + k * gSubgridRows * (cellSize + CELL_SPACING)
            - (CELL_SPACING / 2) - (LINE_THICKNESS / 2);
        HWND hLine = CreateWindowEx(0, _T("STATIC"), NULL,
            WS_CHILD | WS_VISIBLE | SS_BLACKRECT,
            offsetX, y, gridWidth, LINE_THICKNESS,
            hParent, NULL, g_hInst, NULL);
        gGridLines.push_back(hLine);
    }
}

void GenerateGrid(HWND hParent)
{
    DestroyGrid(hParent);

    // Get chosen board size
    TCHAR buf[16];
    GetWindowText(hComboSize, buf, 16);
    gBoardSize = _ttoi(buf);
    SetSubgridDimensions(gBoardSize);

    // Compute cellSize so the grid fits within PUZZLE_WIDTH×PUZZLE_HEIGHT
    int possibleCellSizeW = (PUZZLE_WIDTH - (gBoardSize - 1) * CELL_SPACING) / gBoardSize;
    int possibleCellSizeH = (PUZZLE_HEIGHT - (gBoardSize - 1) * CELL_SPACING) / gBoardSize;
    int cellSize = std::min(possibleCellSizeW, possibleCellSizeH);
    if (cellSize < 10)
        cellSize = 10;

    // Create the scaled font
    gCellFont = CreateScaledFont(cellSize);

    // Compute grid total width and height
    int gridWidth = gBoardSize * (cellSize + CELL_SPACING) - CELL_SPACING;
    int gridHeight = gBoardSize * (cellSize + CELL_SPACING) - CELL_SPACING;

    // Center the grid within the puzzle area
    int offsetX = PUZZLE_START_X + (PUZZLE_WIDTH - gridWidth) / 2;
    int offsetY = PUZZLE_START_Y + (PUZZLE_HEIGHT - gridHeight) / 2;

    // Create 2D vector for cells
    gCells.resize(gBoardSize);
    for (int i = 0; i < gBoardSize; i++)
        gCells[i].resize(gBoardSize, NULL);

    // Create each cell as a multiline edit control for vertical centering
    for (int i = 0; i < gBoardSize; i++)
    {
        for (int j = 0; j < gBoardSize; j++)
        {
            int x = offsetX + j * (cellSize + CELL_SPACING);
            int y = offsetY + i * (cellSize + CELL_SPACING);

            HWND hEdit = CreateWindowEx(
                WS_EX_CLIENTEDGE,
                _T("EDIT"),
                _T(""),
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_CENTER | ES_UPPERCASE | WS_TABSTOP,
                x, y, cellSize, cellSize,
                hParent, NULL, g_hInst, NULL
            );
            gCells[i][j] = hEdit;

            // Subclass for auto-advance, Tab/Shift+Tab, and vertical centering
            SetWindowSubclass(hEdit, EditSubclassProc, 0, 0);

            // Apply the scaled font
            SendMessage(hEdit, WM_SETFONT, (WPARAM)gCellFont, TRUE);
        }
    }
    // Create subgrid lines with the same offsets
    CreateGridLines(hParent, cellSize, offsetX, offsetY);
}

// -----------------------------------------------------------------------------
// Clear, Read, Update Functions
// -----------------------------------------------------------------------------
void ClearGrid()
{
    for (int i = 0; i < gBoardSize; i++)
        for (int j = 0; j < gBoardSize; j++)
            SetWindowText(gCells[i][j], _T(""));
}

std::vector<int> ReadBoard()
{
    std::vector<int> board(gBoardSize * gBoardSize, 0);
    for (int i = 0; i < gBoardSize; i++)
    {
        for (int j = 0; j < gBoardSize; j++)
        {
            TCHAR buf[16];
            GetWindowText(gCells[i][j], buf, 16);
            int val = charToValue(buf[0], gBoardSize);
            board[i * gBoardSize + j] = val;
        }
    }
    return board;
}

void UpdateBoardCell(int row, int col, int val)
{
    TCHAR c = valueToChar(val, gBoardSize);
    TCHAR buf[2] = { c, _T('\0') };
    SetWindowText(gCells[row][col], buf);
}

void UpdateBoardAll(const std::vector<int>& board)
{
    for (int i = 0; i < gBoardSize; i++)
    {
        for (int j = 0; j < gBoardSize; j++)
        {
            int val = board[i * gBoardSize + j];
            UpdateBoardCell(i, j, val);
        }
    }
}

// -----------------------------------------------------------------------------
// Solver Functions
// -----------------------------------------------------------------------------
bool isSafe(const std::vector<int>& board, int boardSize,
    int subRows, int subCols, int row, int col, int num)
{
    if (gStopRequested)
        return false;
    for (int i = 0; i < boardSize; i++)
    {
        if (board[row * boardSize + i] == num || board[i * boardSize + col] == num)
            return false;
    }
    int startRow = row - (row % subRows);
    int startCol = col - (col % subCols);
    for (int r = 0; r < subRows; r++)
    {
        for (int c = 0; c < subCols; c++)
        {
            if (board[(startRow + r) * boardSize + (startCol + c)] == num)
                return false;
        }
    }
    return true;
}

void PumpMessages()
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            return;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

bool solveSudoku(std::vector<int>& board, int boardSize,
    int subRows, int subCols, int row = 0, int col = 0)
{
    if (gStopRequested)
        return false;
    if (row == boardSize)
        return true;
    if (col == boardSize)
        return solveSudoku(board, boardSize, subRows, subCols, row + 1, 0);
    if (board[row * boardSize + col] != 0)
        return solveSudoku(board, boardSize, subRows, subCols, row, col + 1);

    for (int num = 1; num <= boardSize; num++)
    {
        if (gStopRequested)
            return false;
        if (isSafe(board, boardSize, subRows, subCols, row, col, num))
        {
            board[row * boardSize + col] = num;
            UpdateBoardCell(row, col, num);
            PumpMessages();
            if (solveSudoku(board, boardSize, subRows, subCols, row, col + 1))
                return true;
        }
        board[row * boardSize + col] = 0;
        UpdateBoardCell(row, col, 0);
        PumpMessages();
    }
    return false;
}

// -----------------------------------------------------------------------------
// UI State Management
// -----------------------------------------------------------------------------
void SetSolvingUIState(bool solving)
{
    EnableWindow(hButtonGenerate, !solving);
    EnableWindow(hButtonClear, !solving);
    EnableWindow(hComboSize, !solving);

    if (solving)
        SetWindowText(hButtonSolve, _T("Stop"));
    else
        SetWindowText(hButtonSolve, _T("Solve"));
}

// -----------------------------------------------------------------------------
// Window Procedure
// -----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_CONTROLPARENT);
        g_hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        // Create combo box
        hComboSize = CreateWindowEx(WS_EX_CLIENTEDGE, _T("COMBOBOX"), _T("9"),
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
            10, 11, 70, 200, hwnd, (HMENU)IDC_COMBO_SIZE, g_hInst, NULL);
        SendMessage(hComboSize, CB_ADDSTRING, 0, (LPARAM)_T("4x4"));
        SendMessage(hComboSize, CB_ADDSTRING, 0, (LPARAM)_T("6x6"));
        SendMessage(hComboSize, CB_ADDSTRING, 0, (LPARAM)_T("9x9"));
        SendMessage(hComboSize, CB_ADDSTRING, 0, (LPARAM)_T("16x16"));
        SendMessage(hComboSize, CB_SETCURSEL, 2, 0);

        // Create buttons
        hButtonGenerate = CreateWindow(_T("BUTTON"), _T("Generate"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
            90, 10, 80, 25, hwnd, (HMENU)IDC_GENERATE_BUTTON, g_hInst, NULL);
        hButtonClear = CreateWindow(_T("BUTTON"), _T("Clear"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
            180, 10, 80, 25, hwnd, (HMENU)IDC_CLEAR_BUTTON, g_hInst, NULL);
        hButtonSolve = CreateWindow(_T("BUTTON"), _T("Solve"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
            270, 10, 80, 25, hwnd, (HMENU)IDC_SOLVE_BUTTON, g_hInst, NULL);

        GenerateGrid(hwnd);
    }
    break;

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_GENERATE_BUTTON:
            GenerateGrid(hwnd);
            break;
        case IDC_CLEAR_BUTTON:
            ClearGrid();
            break;
        case IDC_SOLVE_BUTTON:
        {
            if (!gIsSolving)
            {
                gIsSolving = true;
                gStopRequested = false;
                SetSolvingUIState(true);
                std::vector<int> board = ReadBoard();
                bool result = solveSudoku(board, gBoardSize, gSubgridRows, gSubgridCols);
                if (!gStopRequested)
                {
                    if (result)
                    {
                        UpdateBoardAll(board);
                        MessageBox(hwnd, _T("Sudoku solved!"), _T("Success"), MB_OK);
                    }
                    else
                    {
                        MessageBox(hwnd, _T("No solution found!"), _T("Error"), MB_OK);
                    }
                }
                else
                {
                    MessageBox(hwnd, _T("Solving stopped."), _T("Info"), MB_OK);
                }
                gIsSolving = false;
                SetSolvingUIState(false);
            }
            else
            {
                gStopRequested = true;
            }
        }
        break;
        }
    }
    break;

    case WM_DESTROY:
        DestroyGrid(hwnd);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// -----------------------------------------------------------------------------
// Entry Point (with IsDialogMessage for Tab Navigation)
// -----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    const TCHAR szClassName[] = _T("SudokuSolverClass");

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = szClassName;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClass(&wc))
        return 0;

    HWND hwnd = CreateWindowEx(
        0,
        szClassName,
        _T("Sudoku Solver"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        535, 600,
        NULL, NULL,
        hInstance, NULL
    );

    if (!hwnd)
        return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Use IsDialogMessage for Tab navigation
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!IsDialogMessage(hwnd, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}