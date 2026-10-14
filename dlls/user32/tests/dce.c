/*
 * Unit tests for DCE support
 *
 * Copyright 2005 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"

#include "wine/test.h"

#ifndef DCX_USESTYLE
#define DCX_USESTYLE         0x00010000
#endif

static HWND hwnd_cache, hwnd_owndc, hwnd_classdc, hwnd_classdc2, hwnd_parent, hwnd_parentdc;

static DPI_AWARENESS_CONTEXT (WINAPI *pSetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);
static UINT (WINAPI *pGetDpiForSystem)(void);

/* test behavior of DC attributes with various GetDC/ReleaseDC combinations */
static void test_dc_attributes(void)
{
    HDC hdc, old_hdc;
    HDC hdcs[20];
    INT i, rop, def_rop, caps;
    BOOL found_dc;

    /* test cache DC */

    hdc = GetDC( hwnd_cache );
    def_rop = GetROP2( hdc );

    SetROP2( hdc, R2_WHITE );
    rop = GetROP2( hdc );
    ok( rop == R2_WHITE, "wrong ROP2 %d\n", rop );

    ok( WindowFromDC( hdc ) == hwnd_cache, "wrong window\n" );
    ReleaseDC( hwnd_cache, hdc );
    ok( WindowFromDC( hdc ) != hwnd_cache, "wrong window\n" );
    hdc = GetDC( hwnd_cache );
    rop = GetROP2( hdc );
    ok( rop == def_rop, "wrong ROP2 %d after release\n", rop );
    SetROP2( hdc, R2_WHITE );
    ok( WindowFromDC( hdc ) == hwnd_cache, "wrong window\n" );
    ReleaseDC( hwnd_cache, hdc );
    old_hdc = hdc;

    found_dc = FALSE;
    for (i = 0; i < 20; i++)
    {
        hdc = hdcs[i] = GetDCEx( hwnd_cache, 0, DCX_USESTYLE | DCX_NORESETATTRS );
        if (!hdc) break;
        rop = GetROP2( hdc );
        ok( rop == def_rop, "wrong ROP2 %d after release %p/%p\n", rop, old_hdc, hdc );
        if (hdc == old_hdc)
        {
            found_dc = TRUE;
            SetROP2( hdc, R2_WHITE );
        }
    }
    if (!found_dc)
    {
        trace( "hdc %p not found in cache using %p\n", old_hdc, hdcs[0] );
        old_hdc = hdcs[0];
        SetROP2( old_hdc, R2_WHITE );
    }
    while (i > 0) ReleaseDC( hwnd_cache, hdcs[--i] );

    for (i = 0; i < 20; i++)
    {
        hdc = hdcs[i] = GetDCEx( hwnd_cache, 0, DCX_USESTYLE | DCX_NORESETATTRS );
        if (!hdc) break;
        rop = GetROP2( hdc );
        if (hdc == old_hdc)
            ok( rop == R2_WHITE || broken( rop == def_rop),  /* win9x doesn't support DCX_NORESETATTRS */
                "wrong ROP2 %d after release %p/%p\n", rop, old_hdc, hdc );
        else
            ok( rop == def_rop, "wrong ROP2 %d after release %p/%p\n", rop, old_hdc, hdc );
    }
    while (i > 0) ReleaseDC( hwnd_cache, hdcs[--i] );

    for (i = 0; i < 20; i++)
    {
        hdc = hdcs[i] = GetDCEx( hwnd_cache, 0, DCX_USESTYLE );
        if (!hdc) break;
        rop = GetROP2( hdc );
        if (hdc == old_hdc)
        {
            ok( rop == R2_WHITE || broken( rop == def_rop),
                "wrong ROP2 %d after release %p/%p\n", rop, old_hdc, hdc );
            SetROP2( old_hdc, def_rop );
        }
        else
            ok( rop == def_rop, "wrong ROP2 %d after release %p/%p\n", rop, old_hdc, hdc );
    }
    while (i > 0) ReleaseDC( hwnd_cache, hdcs[--i] );

    /* Released cache DCs are 'disabled' */
    rop = SetROP2( old_hdc, R2_BLACK );
    ok( rop == 0, "got %d\n", rop );
    rop = GetROP2( old_hdc );
    ok( rop == 0, "got %d\n", rop );
    caps = GetDeviceCaps( old_hdc, HORZRES );
    ok( caps == 0, "got %d\n", caps );
    caps = GetDeviceCaps( old_hdc, VERTRES );
    ok( caps == 0, "got %d\n", caps );
    caps = GetDeviceCaps( old_hdc, NUMCOLORS );
    ok( caps == 0, "got %d\n", caps );
    ok( WindowFromDC( old_hdc ) != hwnd_cache, "wrong window\n" );

    hdc = GetDC(0);
    caps = GetDeviceCaps( hdc, HORZRES );
    ok( caps != 0, "got %d\n", caps );
    caps = GetDeviceCaps( hdc, VERTRES );
    ok( caps != 0, "got %d\n", caps );
    caps = GetDeviceCaps( hdc, NUMCOLORS );
    ok( caps != 0, "got %d\n", caps );
    ReleaseDC( 0, hdc );
    caps = GetDeviceCaps( hdc, HORZRES );
    ok( caps == 0, "got %d\n", caps );
    caps = GetDeviceCaps( hdc, VERTRES );
    ok( caps == 0, "got %d\n", caps );
    caps = GetDeviceCaps( hdc, NUMCOLORS );
    ok( caps == 0, "got %d\n", caps );

    /* test own DC */

    hdc = GetDC( hwnd_owndc );
    SetROP2( hdc, R2_WHITE );
    rop = GetROP2( hdc );
    ok( rop == R2_WHITE, "wrong ROP2 %d\n", rop );

    old_hdc = hdc;
    ok( WindowFromDC( hdc ) == hwnd_owndc, "wrong window\n" );
    ReleaseDC( hwnd_owndc, hdc );
    ok( WindowFromDC( hdc ) == hwnd_owndc, "wrong window\n" );
    hdc = GetDC( hwnd_owndc );
    ok( old_hdc == hdc, "didn't get same DC %p/%p\n", old_hdc, hdc );
    rop = GetROP2( hdc );
    ok( rop == R2_WHITE, "wrong ROP2 %d after release\n", rop );
    ok( WindowFromDC( hdc ) == hwnd_owndc, "wrong window\n" );
    ReleaseDC( hwnd_owndc, hdc );
    rop = GetROP2( hdc );
    ok( rop == R2_WHITE, "wrong ROP2 %d after second release\n", rop );

    /* test class DC */

    hdc = GetDC( hwnd_classdc );
    SetROP2( hdc, R2_WHITE );
    rop = GetROP2( hdc );
    ok( rop == R2_WHITE, "wrong ROP2 %d\n", rop );

    old_hdc = hdc;
    ok( WindowFromDC( hdc ) == hwnd_classdc, "wrong window\n" );
    ReleaseDC( hwnd_classdc, hdc );
    ok( WindowFromDC( hdc ) == hwnd_classdc, "wrong window\n" );
    hdc = GetDC( hwnd_classdc );
    ok( old_hdc == hdc, "didn't get same DC %p/%p\n", old_hdc, hdc );
    rop = GetROP2( hdc );
    ok( rop == R2_WHITE, "wrong ROP2 %d after release\n", rop );
    ok( WindowFromDC( hdc ) == hwnd_classdc, "wrong window\n" );
    ReleaseDC( hwnd_classdc, hdc );
    rop = GetROP2( hdc );
    ok( rop == R2_WHITE, "wrong ROP2 %d after second release\n", rop );

    /* test class DC with 2 windows */

    old_hdc = GetDC( hwnd_classdc );
    SetROP2( old_hdc, R2_BLACK );
    ok( WindowFromDC( old_hdc ) == hwnd_classdc, "wrong window\n" );
    hdc = GetDC( hwnd_classdc2 );
    ok( old_hdc == hdc, "didn't get same DC %p/%p\n", old_hdc, hdc );
    rop = GetROP2( hdc );
    ok( rop == R2_BLACK, "wrong ROP2 %d for other window\n", rop );
    ok( WindowFromDC( hdc ) == hwnd_classdc2, "wrong window\n" );
    ReleaseDC( hwnd_classdc, old_hdc );
    ReleaseDC( hwnd_classdc, hdc );
    ok( WindowFromDC( hdc ) == hwnd_classdc2, "wrong window\n" );
    rop = GetROP2( hdc );
    ok( rop == R2_BLACK, "wrong ROP2 %d after release\n", rop );
}


/* test behavior with various invalid parameters */
static void test_parameters(void)
{
    HDC hdc;

    hdc = GetDC( hwnd_cache );
    ok( ReleaseDC( hwnd_owndc, hdc ), "ReleaseDC with wrong window should succeed\n" );

    hdc = GetDC( hwnd_cache );
    ok( !ReleaseDC( hwnd_cache, 0 ), "ReleaseDC with wrong HDC should fail\n" );
    ok( ReleaseDC( hwnd_cache, hdc ), "correct ReleaseDC should succeed\n" );
    ok( !ReleaseDC( hwnd_cache, hdc ), "second ReleaseDC should fail\n" );

    hdc = GetDC( hwnd_owndc );
    ok( ReleaseDC( hwnd_cache, hdc ), "ReleaseDC with wrong window should succeed\n" );
    hdc = GetDC( hwnd_owndc );
    ok( ReleaseDC( hwnd_owndc, hdc ), "correct ReleaseDC should succeed\n" );
    ok( ReleaseDC( hwnd_owndc, hdc ), "second ReleaseDC should succeed\n" );

    hdc = GetDC( hwnd_classdc );
    ok( ReleaseDC( hwnd_cache, hdc ), "ReleaseDC with wrong window should succeed\n" );
    hdc = GetDC( hwnd_classdc );
    ok( ReleaseDC( hwnd_classdc, hdc ), "correct ReleaseDC should succeed\n" );
    ok( ReleaseDC( hwnd_classdc, hdc ), "second ReleaseDC should succeed\n" );
}


static void test_dc_visrgn(void)
{
    HDC old_hdc, hdc;
    HRGN hrgn, hrgn2;
    RECT rect, parent_rect;

    /* cache DC */

    SetRect( &rect, 10, 10, 20, 20 );
    MapWindowPoints( hwnd_cache, 0, (POINT *)&rect, 2 );
    hrgn = CreateRectRgnIndirect( &rect );
    hdc = GetDCEx( hwnd_cache, hrgn, DCX_INTERSECTRGN | DCX_USESTYLE );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    ok( GetRgnBox( hrgn, &rect ) != ERROR, "region must still be valid\n" );
    ReleaseDC( hwnd_cache, hdc );
    ok( GetRgnBox( hrgn, &rect ) == ERROR, "region must no longer be valid\n" );

    /* cache DC with NORESETATTRS */

    SetRect( &rect, 10, 10, 20, 20 );
    MapWindowPoints( hwnd_cache, 0, (POINT *)&rect, 2 );
    hrgn = CreateRectRgnIndirect( &rect );
    hdc = GetDCEx( hwnd_cache, hrgn, DCX_INTERSECTRGN | DCX_USESTYLE | DCX_NORESETATTRS );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    ok( GetRgnBox( hrgn, &rect ) != ERROR, "region must still be valid\n" );
    ReleaseDC( hwnd_cache, hdc );
    ok( GetRgnBox( hrgn, &rect ) == ERROR, "region must no longer be valid\n" );
    hdc = GetDCEx( hwnd_cache, 0, DCX_USESTYLE | DCX_NORESETATTRS );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( !(rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20),
        "clip box should have been reset %s\n", wine_dbgstr_rect( &rect ));
    ReleaseDC( hwnd_cache, hdc );

    /* window DC */

    SetRect( &rect, 10, 10, 20, 20 );
    MapWindowPoints( hwnd_owndc, 0, (POINT *)&rect, 2 );
    hrgn = CreateRectRgnIndirect( &rect );
    hdc = GetDCEx( hwnd_owndc, hrgn, DCX_INTERSECTRGN | DCX_USESTYLE );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    ok( GetRgnBox( hrgn, &rect ) != ERROR, "region must still be valid\n" );
    ReleaseDC( hwnd_owndc, hdc );
    ok( GetRgnBox( hrgn, &rect ) != ERROR, "region must still be valid\n" );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    hdc = GetDCEx( hwnd_owndc, 0, DCX_USESTYLE );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    ok( GetRgnBox( hrgn, &rect ) != ERROR, "region must still be valid\n" );
    ReleaseDC( hwnd_owndc, hdc );
    ok( GetRgnBox( hrgn, &rect ) != ERROR, "region must still be valid\n" );

    SetRect( &rect, 20, 20, 30, 30 );
    MapWindowPoints( hwnd_owndc, 0, (POINT *)&rect, 2 );
    hrgn2 = CreateRectRgnIndirect( &rect );
    hdc = GetDCEx( hwnd_owndc, hrgn2, DCX_INTERSECTRGN | DCX_USESTYLE );
    ok( GetRgnBox( hrgn, &rect ) == ERROR, "region must no longer be valid\n" );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 20 && rect.top >= 20 && rect.right <= 30 && rect.bottom <= 30,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    ok( GetRgnBox( hrgn2, &rect ) != ERROR, "region2 must still be valid\n" );
    ReleaseDC( hwnd_owndc, hdc );
    ok( GetRgnBox( hrgn2, &rect ) != ERROR, "region2 must still be valid\n" );
    hdc = GetDCEx( hwnd_owndc, 0, DCX_EXCLUDERGN | DCX_USESTYLE );
    ok( GetRgnBox( hrgn2, &rect ) == ERROR, "region must no longer be valid\n" );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( !(rect.left >= 20 && rect.top >= 20 && rect.right <= 30 && rect.bottom <= 30),
        "clip box should have been reset %s\n", wine_dbgstr_rect( &rect ));
    ReleaseDC( hwnd_owndc, hdc );

    /* class DC */

    SetRect( &rect, 10, 10, 20, 20 );
    MapWindowPoints( hwnd_classdc, 0, (POINT *)&rect, 2 );
    hrgn = CreateRectRgnIndirect( &rect );
    hdc = GetDCEx( hwnd_classdc, hrgn, DCX_INTERSECTRGN | DCX_USESTYLE );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    ok( GetRgnBox( hrgn, &rect ) != ERROR, "region must still be valid\n" );
    ReleaseDC( hwnd_classdc, hdc );
    ok( GetRgnBox( hrgn, &rect ) != ERROR, "region must still be valid\n" );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));

    hdc = GetDCEx( hwnd_classdc, 0, DCX_USESTYLE );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    ok( GetRgnBox( hrgn, &rect ) != ERROR, "region must still be valid\n" );
    ReleaseDC( hwnd_classdc, hdc );
    ok( GetRgnBox( hrgn, &rect ) != ERROR, "region must still be valid\n" );

    SetRect( &rect, 20, 20, 30, 30 );
    MapWindowPoints( hwnd_classdc, 0, (POINT *)&rect, 2 );
    hrgn2 = CreateRectRgnIndirect( &rect );
    hdc = GetDCEx( hwnd_classdc, hrgn2, DCX_INTERSECTRGN | DCX_USESTYLE );
    ok( GetRgnBox( hrgn, &rect ) == ERROR, "region must no longer be valid\n" );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 20 && rect.top >= 20 && rect.right <= 30 && rect.bottom <= 30,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    ok( GetRgnBox( hrgn2, &rect ) != ERROR, "region2 must still be valid\n" );

    old_hdc = hdc;
    hdc = GetDCEx( hwnd_classdc2, 0, DCX_USESTYLE );
    ok( old_hdc == hdc, "did not get the same hdc %p/%p\n", old_hdc, hdc );
    ok( GetRgnBox( hrgn2, &rect ) != ERROR, "region2 must still be valid\n" );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( !(rect.left >= 20 && rect.top >= 20 && rect.right <= 30 && rect.bottom <= 30),
        "clip box should have been reset %s\n", wine_dbgstr_rect( &rect ));
    ReleaseDC( hwnd_classdc2, hdc );
    ok( GetRgnBox( hrgn2, &rect ) != ERROR, "region2 must still be valid\n" );
    hdc = GetDCEx( hwnd_classdc2, 0, DCX_EXCLUDERGN | DCX_USESTYLE );
    ok( GetRgnBox( hrgn2, &rect ) != ERROR, "region2 must still be valid\n" );
    ok( !(rect.left >= 20 && rect.top >= 20 && rect.right <= 30 && rect.bottom <= 30),
        "clip box must have been reset %s\n", wine_dbgstr_rect( &rect ));
    ReleaseDC( hwnd_classdc2, hdc );

    /* parent DC */
    hdc = GetDC( hwnd_parentdc );
    GetClipBox( hdc, &rect );
    MapWindowPoints(hwnd_parentdc, hwnd_parent, (POINT *)&rect, 2);
    ReleaseDC( hwnd_parentdc, hdc );

    hdc = GetDC( hwnd_parent );
    GetClipBox( hdc, &parent_rect );
    ReleaseDC( hwnd_parent, hdc );

    ok( EqualRect( &rect, &parent_rect ), "rect = %s, expected %s\n", wine_dbgstr_rect( &rect ),
        wine_dbgstr_rect( &parent_rect ));
}


/* test various BeginPaint/EndPaint behaviors */
static void test_begin_paint(void)
{
    HDC old_hdc, hdc;
    RECT rect, parent_rect, client_rect;
    PAINTSTRUCT ps;
    COLORREF cr;

    /* cache DC */

    /* clear update region */
    RedrawWindow( hwnd_cache, NULL, 0, RDW_VALIDATE|RDW_NOFRAME|RDW_NOERASE );
    SetRect( &rect, 10, 10, 20, 20 );
    RedrawWindow( hwnd_cache, &rect, 0, RDW_INVALIDATE );
    hdc = BeginPaint( hwnd_cache, &ps );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    EndPaint( hwnd_cache, &ps );

    SetWindowPos( hwnd_cache, 0, 0, 0, 100, 100, SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE );
    RedrawWindow( hwnd_cache, NULL, 0, RDW_VALIDATE|RDW_NOFRAME|RDW_NOERASE );
    SetRect( &rect, 0, 0, 150, 150 );
    RedrawWindow( hwnd_cache, &rect, 0, RDW_INVALIDATE|RDW_NOFRAME|RDW_NOERASE );
    hdc = BeginPaint( hwnd_cache, &ps );
    GetClipBox( hdc, &rect );
    GetClientRect( hwnd_cache, &client_rect );
    ok( EqualRect( &rect, &client_rect ), "clip box = %s, expected %s\n",
            wine_dbgstr_rect( &rect ), wine_dbgstr_rect( &client_rect ));
    SetWindowPos( hwnd_cache, 0, 0, 0, 200, 200, SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE );
    GetClipBox( hdc, &rect );
    GetClientRect( hwnd_cache, &client_rect );
    todo_wine ok( (!rect.left && !rect.top && rect.right == 150 && rect.bottom == 150) ||
            broken( EqualRect( &rect, &client_rect )),
            "clip box = %s\n", wine_dbgstr_rect( &rect ));
    EndPaint( hwnd_cache, &ps );

    SetWindowPos( hwnd_cache, 0, 0, 0, 100, 100, SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE );
    RedrawWindow( hwnd_cache, NULL, 0, RDW_INVALIDATE|RDW_NOFRAME|RDW_NOERASE );
    hdc = BeginPaint( hwnd_cache, &ps );
    SetWindowPos( hwnd_cache, 0, 0, 0, 200, 200, SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE );
    GetClipBox( hdc, &rect );
    GetClientRect( hwnd_cache, &client_rect );
    todo_wine ok( EqualRect( &rect, &client_rect ), "clip box = %s, expected %s\n",
            wine_dbgstr_rect( &rect ), wine_dbgstr_rect( &client_rect ));
    EndPaint( hwnd_cache, &ps );

    /* window DC */

    RedrawWindow( hwnd_owndc, NULL, 0, RDW_VALIDATE|RDW_NOFRAME|RDW_NOERASE );
    SetRect( &rect, 10, 10, 20, 20 );
    RedrawWindow( hwnd_owndc, &rect, 0, RDW_INVALIDATE );
    hdc = BeginPaint( hwnd_owndc, &ps );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    ReleaseDC( hwnd_owndc, hdc );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    ok( GetDC( hwnd_owndc ) == hdc, "got different hdc\n" );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    EndPaint( hwnd_owndc, &ps );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( !(rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20),
        "clip box should have been reset %s\n", wine_dbgstr_rect( &rect ));
    RedrawWindow( hwnd_owndc, NULL, 0, RDW_VALIDATE|RDW_NOFRAME|RDW_NOERASE );
    SetRect( &rect, 10, 10, 20, 20 );
    RedrawWindow( hwnd_owndc, &rect, 0, RDW_INVALIDATE|RDW_ERASE );
    ok( GetDC( hwnd_owndc ) == hdc, "got different hdc\n" );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( !(rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20),
        "clip box should be the whole window %s\n", wine_dbgstr_rect( &rect ));
    RedrawWindow( hwnd_owndc, NULL, 0, RDW_ERASENOW );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( !(rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20),
        "clip box should still be the whole window %s\n", wine_dbgstr_rect( &rect ));

    SetWindowPos( hwnd_owndc, 0, 0, 0, 100, 100, SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE );
    RedrawWindow( hwnd_owndc, NULL, 0, RDW_VALIDATE|RDW_NOFRAME|RDW_NOERASE );
    SetRect( &rect, 0, 0, 50, 50 );
    RedrawWindow( hwnd_owndc, &rect, 0, RDW_INVALIDATE|RDW_ERASE );
    hdc = BeginPaint( hwnd_owndc, &ps );
    GetClipBox( hdc, &rect );
    ok( !rect.left && !rect.top && rect.right == 50 && rect.bottom == 50,
            "clip box = %s\n", wine_dbgstr_rect( &rect ));
    SetWindowPos( hwnd_owndc, 0, 0, 0, 200, 200, SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE );
    GetClipBox( hdc, &rect );
    GetClientRect( hwnd_owndc, &client_rect );
    ok( EqualRect( &rect, &client_rect ), "clip box = %s, expected %s\n",
            wine_dbgstr_rect( &rect ), wine_dbgstr_rect( &client_rect ));
    EndPaint( hwnd_owndc, &ps );

    /* class DC */

    RedrawWindow( hwnd_classdc, NULL, 0, RDW_VALIDATE|RDW_NOFRAME|RDW_NOERASE );
    SetRect( &rect, 10, 10, 20, 20 );
    RedrawWindow( hwnd_classdc, &rect, 0, RDW_INVALIDATE );
    hdc = BeginPaint( hwnd_classdc, &ps );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));

    old_hdc = hdc;
    hdc = GetDC( hwnd_classdc2 );
    ok( old_hdc == hdc, "did not get the same hdc %p/%p\n", old_hdc, hdc );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( !(rect.left >= 10 && rect.top >= 10 && rect.right <= 20 && rect.bottom <= 20),
        "clip box should have been reset %s\n", wine_dbgstr_rect( &rect ));
    ReleaseDC( hwnd_classdc2, hdc );
    EndPaint( hwnd_classdc, &ps );

    /* parent DC */
    RedrawWindow( hwnd_parent, NULL, 0, RDW_VALIDATE|RDW_NOFRAME|RDW_NOERASE );
    RedrawWindow( hwnd_parentdc, NULL, 0, RDW_INVALIDATE );
    hdc = BeginPaint( hwnd_parentdc, &ps );
    GetClipBox( hdc, &rect );
    MapWindowPoints(hwnd_parentdc, hwnd_parent, (POINT *)&rect, 2);
    cr = SetPixel( hdc, 10, 10, RGB(255, 0, 0) );
    ok( cr != -1, "error drawing outside of window client area\n" );
    EndPaint( hwnd_parentdc, &ps );
    GetClientRect( hwnd_parent, &parent_rect );

    todo_wine ok( rect.left == parent_rect.left, "rect.left = %ld, expected %ld\n", rect.left, parent_rect.left );
    todo_wine ok( rect.top == parent_rect.top, "rect.top = %ld, expected %ld\n", rect.top, parent_rect.top );
    todo_wine ok( rect.right == parent_rect.right, "rect.right = %ld, expected %ld\n", rect.right, parent_rect.right );
    todo_wine ok( rect.bottom == parent_rect.bottom, "rect.bottom = %ld, expected %ld\n", rect.bottom, parent_rect.bottom );

    hdc = GetDC( hwnd_parent );
    todo_wine ok( GetPixel( hdc, 60, 60 ) == cr, "error drawing outside of window client area\n" );
    ReleaseDC( hwnd_parent, hdc );
}

static void test_cropped_parentdc_paint_clipbox(void)
{
    /* Use WS_BORDER to differentiate window and client rects */
    const UINT common_style = WS_VISIBLE | WS_BORDER;
    HWND hwnd_toplevel, hwnd_container, hwnd_child;
    RECT parent_rect, expect_rect, toplevel_rect, rect;
    POINT dot_pos = { 10, 10 };
    PAINTSTRUCT ps;
    COLORREF cr;
    HDC hdc;

    hwnd_toplevel = CreateWindowA( "static", NULL, WS_OVERLAPPED | common_style,
                                   400, 200, 100, 100, NULL, NULL, NULL, NULL );

    /* Container window extends "outside" the top-level window (hence cropped) */
    hwnd_container = CreateWindowA( "static", NULL, WS_CHILD | common_style,
                                    -40, -40, 180, 180, hwnd_toplevel, NULL, NULL, NULL );

    hwnd_child = CreateWindowA( "parentdc_class", NULL, WS_CHILD | common_style,
                                50, 50, 3, 3, hwnd_container, NULL, NULL, NULL );

    RedrawWindow( hwnd_toplevel, NULL, 0, RDW_VALIDATE|RDW_NOFRAME|RDW_NOERASE );
    RedrawWindow( hwnd_child, NULL, 0, RDW_INVALIDATE );

    hdc = BeginPaint( hwnd_child, &ps );
    GetClipBox( hdc, &rect );
    cr = SetPixel( hdc, dot_pos.x, dot_pos.y, RGB(255, 0, 0) );
    ok( cr != -1, "error drawing outside of window client area\n" );
    EndPaint( hwnd_child, &ps );

    GetClientRect( hwnd_toplevel, &toplevel_rect );
    MapWindowPoints( hwnd_toplevel, hwnd_container, (POINT *)&toplevel_rect, 2);
    GetClientRect( hwnd_container, &parent_rect );
    IntersectRect( &expect_rect, &toplevel_rect, &parent_rect );

    MapWindowPoints( hwnd_child, hwnd_container, (POINT *)&rect, 2 );
    todo_wine
    ok( EqualRect( &rect, &expect_rect ), "rect = %s, expected %s\n",
        wine_dbgstr_rect( &rect ), wine_dbgstr_rect( &expect_rect ) );

    hdc = GetDC( hwnd_container );
    MapWindowPoints( hwnd_child, hwnd_container, &dot_pos, 1 );
    todo_wine
    ok( GetPixel( hdc, dot_pos.x, dot_pos.y ) == cr, "error drawing outside of window client area\n" );
    ReleaseDC( hwnd_container, hdc );

    DestroyWindow( hwnd_toplevel );
}

/* test ScrollWindow with window DCs */
static void test_scroll_window(void)
{
    PAINTSTRUCT ps;
    HDC hdc;
    RECT clip, rect;

    /* ScrollWindow uses the window DC, ScrollWindowEx doesn't */

    UpdateWindow( hwnd_owndc );
    SetRect( &clip, 25, 25, 50, 50 );
    ScrollWindow( hwnd_owndc, -5, -10, NULL, &clip );
    hdc = BeginPaint( hwnd_owndc, &ps );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 25 && rect.top >= 25 && rect.right <= 50 && rect.bottom <= 50,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    EndPaint( hwnd_owndc, &ps );

    SetViewportExtEx( hdc, 2, 3, NULL );
    SetViewportOrgEx( hdc, 30, 20, NULL );

    ScrollWindow( hwnd_owndc, -5, -10, NULL, &clip );
    hdc = BeginPaint( hwnd_owndc, &ps );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 25 && rect.top >= 25 && rect.right <= 50 && rect.bottom <= 50,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    EndPaint( hwnd_owndc, &ps );

    ScrollWindowEx( hwnd_owndc, -5, -10, NULL, &clip, 0, NULL, SW_INVALIDATE | SW_ERASE );
    hdc = BeginPaint( hwnd_owndc, &ps );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= -5 && rect.top >= 5 && rect.right <= 20 && rect.bottom <= 30,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    EndPaint( hwnd_owndc, &ps );

    SetViewportExtEx( hdc, 1, 1, NULL );
    SetViewportOrgEx( hdc, 0, 0, NULL );

    ScrollWindowEx( hwnd_owndc, -5, -10, NULL, &clip, 0, NULL, SW_INVALIDATE | SW_ERASE );
    hdc = BeginPaint( hwnd_owndc, &ps );
    SetRectEmpty( &rect );
    GetClipBox( hdc, &rect );
    ok( rect.left >= 25 && rect.top >= 25 && rect.right <= 50 && rect.bottom <= 50,
        "invalid clip box %s\n", wine_dbgstr_rect( &rect ));
    EndPaint( hwnd_owndc, &ps );
}

static void test_invisible_create(void)
{
    HWND hwnd_owndc = CreateWindowA("owndc_class", NULL, WS_OVERLAPPED,
                                    0, 200, 100, 100,
                                    0, 0, GetModuleHandleA(0), NULL );
    HDC dc1, dc2;

    dc1 = GetDC(hwnd_owndc);
    dc2 = GetDC(hwnd_owndc);

    ok(dc1 == dc2, "expected owndc dcs to match\n");

    ReleaseDC(hwnd_owndc, dc2);
    ReleaseDC(hwnd_owndc, dc1);
    DestroyWindow(hwnd_owndc);
}

static void test_dc_layout(void)
{
    HWND hwnd_cache_rtl, hwnd_owndc_rtl, hwnd_classdc_rtl, hwnd_classdc2_rtl;
    HDC hdc;
    DWORD layout;

    hdc = GetDC( hwnd_cache );
    SetLayout( hdc, LAYOUT_RTL );
    layout = GetLayout( hdc );
    ReleaseDC( hwnd_cache, hdc );
    if (!layout)
    {
        win_skip( "SetLayout not supported\n" );
        return;
    }

    hwnd_cache_rtl = CreateWindowExA(WS_EX_LAYOUTRTL, "cache_class", NULL, WS_OVERLAPPED | WS_VISIBLE,
                                     0, 0, 100, 100, 0, 0, GetModuleHandleA(0), NULL );
    hwnd_owndc_rtl = CreateWindowExA(WS_EX_LAYOUTRTL, "owndc_class", NULL, WS_OVERLAPPED | WS_VISIBLE,
                                     0, 200, 100, 100, 0, 0, GetModuleHandleA(0), NULL );
    hwnd_classdc_rtl = CreateWindowExA(WS_EX_LAYOUTRTL, "classdc_class", NULL, WS_OVERLAPPED | WS_VISIBLE,
                                       200, 0, 100, 100, 0, 0, GetModuleHandleA(0), NULL );
    hwnd_classdc2_rtl = CreateWindowExA(WS_EX_LAYOUTRTL, "classdc_class", NULL, WS_OVERLAPPED | WS_VISIBLE,
                                        200, 200, 100, 100, 0, 0, GetModuleHandleA(0), NULL );
    hdc = GetDC( hwnd_cache_rtl );
    layout = GetLayout( hdc );

    ok( layout == LAYOUT_RTL, "wrong layout %lx\n", layout );
    SetLayout( hdc, 0 );
    ReleaseDC( hwnd_cache_rtl, hdc );
    hdc = GetDC( hwnd_owndc_rtl );
    layout = GetLayout( hdc );
    ok( layout == LAYOUT_RTL, "wrong layout %lx\n", layout );
    ReleaseDC( hwnd_cache_rtl, hdc );

    hdc = GetDC( hwnd_cache );
    layout = GetLayout( hdc );
    ok( layout == 0, "wrong layout %lx\n", layout );
    ReleaseDC( hwnd_cache, hdc );

    hdc = GetDC( hwnd_owndc_rtl );
    layout = GetLayout( hdc );
    ok( layout == LAYOUT_RTL, "wrong layout %lx\n", layout );
    SetLayout( hdc, 0 );
    ReleaseDC( hwnd_owndc_rtl, hdc );
    hdc = GetDC( hwnd_owndc_rtl );
    layout = GetLayout( hdc );
    ok( layout == LAYOUT_RTL, "wrong layout %lx\n", layout );
    ReleaseDC( hwnd_owndc_rtl, hdc );

    hdc = GetDC( hwnd_classdc_rtl );
    layout = GetLayout( hdc );
    ok( layout == LAYOUT_RTL, "wrong layout %lx\n", layout );
    SetLayout( hdc, 0 );
    ReleaseDC( hwnd_classdc_rtl, hdc );
    hdc = GetDC( hwnd_classdc2_rtl );
    layout = GetLayout( hdc );
    ok( layout == LAYOUT_RTL, "wrong layout %lx\n", layout );
    ReleaseDC( hwnd_classdc2_rtl, hdc );
    hdc = GetDC( hwnd_classdc );
    layout = GetLayout( hdc );
    ok( layout == LAYOUT_RTL, "wrong layout %lx\n", layout );
    ReleaseDC( hwnd_classdc_rtl, hdc );

    DestroyWindow(hwnd_classdc2_rtl);
    DestroyWindow(hwnd_classdc_rtl);
    DestroyWindow(hwnd_owndc_rtl);
    DestroyWindow(hwnd_cache_rtl);
}

struct text_results
{
    BOOL tm_ret;
    BYTE tm[sizeof(TEXTMETRICA)];
    SIZE extent[4];
    int fit;
    int dx[12];
    BOOL width_ret;
    int width['z' - 'A' + 1];
    BOOL abc_ret;
    ABC abc['z' - 'A' + 1];
    int otm[17];
    DWORD glyph_ret;
    GLYPHMETRICS gm;
    DWORD kern_pairs;
};

struct region_results
{
    int ret;
    DWORD size;
    DWORD data[(sizeof(RGNDATAHEADER) + 32 * sizeof(RECT)) / sizeof(DWORD)];
};

struct path_results
{
    int count;
    POINT points[512];
    BYTE types[512];
    int rgn_ret;
    RECT rgn_box;
};

struct dc_results
{
    BOOL update_ret;
    RECT update_rect;
    struct region_results update_rgn;
    RECT paint_rect;
    int caps[4];
    POINT lp_to_dp[5][25];
    POINT dp_to_lp[5][25];
    int clip_ret[8];
    RECT clip_box[8];
    struct region_results clip_rgn[2];
    struct region_results meta_rgn;
    int sysrgn_ret;
    RECT sysrgn_box;
    POINT dc_org;
    BOOL pt_visible[64];
    BOOL rect_visible[64];
    POINT line_pos;
    POINT text_pos;
    struct path_results path[2];
    BOOL scroll_ret;
    RECT scroll_rect;
    int scroll_rgn_ret;
    RECT scroll_rgn_box;
    struct text_results text[5];
};

static const char *dc_names[3] = { "GetDC", "GetDCEx", "BeginPaint" };
static const char *font_names[5] = { "System", "MS Shell Dlg -11", "Tahoma -13", "Tahoma 0", "Tahoma bold" };

/* the Windows behaviour of these with GDI scaling above 96 DPI has not been verified */
static BOOL test_unverified;

static void flush_events(void)
{
    DWORD time = GetTickCount() + 100;
    MSG msg;
    int diff;

    while ((diff = time - GetTickCount()) > 0)
    {
        if (MsgWaitForMultipleObjects( 0, NULL, FALSE, diff, QS_ALLINPUT ) == WAIT_TIMEOUT) break;
        while (PeekMessageA( &msg, 0, 0, 0, PM_REMOVE )) DispatchMessageA( &msg );
    }
}

static void check_results_( int line, const char *name, const void *res, const void *expect, size_t size )
{
    const int *ptr = res, *expect_ptr = expect;
    unsigned int i, count = size / sizeof(int);

    for (i = 0; i < count; i++) if (ptr[i] != expect_ptr[i]) break;
    ok_(__FILE__, line)( i == count, "%s: got %d at index %u, expected %d\n", name,
                         i < count ? ptr[i] : 0, i, i < count ? expect_ptr[i] : 0 );
}
#define check_results( res, expect, field ) \
    check_results_( __LINE__, #field, &(res)->field, &(expect)->field, sizeof((res)->field) )

static void measure_text( HDC hdc, HFONT font, struct text_results *res )
{
    static const char *strings[3] = { "Wine", "GDI scaling", "The quick brown fox jumps over the lazy dog" };
    static const MAT2 mat = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };
    OUTLINETEXTMETRICA *otm;
    TEXTMETRICA tm;
    HFONT old_font;
    UINT i, size;

    old_font = SelectObject( hdc, font );

    memset( &tm, 0, sizeof(tm) );
    res->tm_ret = GetTextMetricsA( hdc, &tm );
    /* skip the trailing padding */
    memcpy( res->tm, &tm, FIELD_OFFSET( TEXTMETRICA, tmCharSet ) + sizeof(tm.tmCharSet) );
    for (i = 0; i < ARRAY_SIZE(strings); i++)
        GetTextExtentPoint32A( hdc, strings[i], strlen( strings[i] ), &res->extent[i] );
    GetTextExtentExPointA( hdc, strings[1], strlen( strings[1] ), 50, &res->fit, res->dx, &res->extent[3] );
    res->width_ret = GetCharWidth32A( hdc, 'A', 'z', res->width );
    res->abc_ret = GetCharABCWidthsA( hdc, 'A', 'z', res->abc );

    size = GetOutlineTextMetricsA( hdc, 0, NULL );
    if (size && (otm = calloc( 1, size )))
    {
        res->otm[0] = GetOutlineTextMetricsA( hdc, size, otm );
        res->otm[1] = otm->otmEMSquare;
        res->otm[2] = otm->otmAscent;
        res->otm[3] = otm->otmDescent;
        res->otm[4] = otm->otmLineGap;
        res->otm[5] = otm->otmsCapEmHeight;
        res->otm[6] = otm->otmsXHeight;
        res->otm[7] = otm->otmrcFontBox.left;
        res->otm[8] = otm->otmrcFontBox.top;
        res->otm[9] = otm->otmrcFontBox.right;
        res->otm[10] = otm->otmrcFontBox.bottom;
        res->otm[11] = otm->otmMacAscent;
        res->otm[12] = otm->otmMacDescent;
        res->otm[13] = otm->otmMacLineGap;
        res->otm[14] = otm->otmusMinimumPPEM;
        res->otm[15] = otm->otmsUnderscoreSize;
        res->otm[16] = otm->otmsUnderscorePosition;
        free( otm );
    }

    res->glyph_ret = GetGlyphOutlineA( hdc, 'g', GGO_METRICS, &res->gm, 0, NULL, &mat );
    res->kern_pairs = GetKerningPairsA( hdc, 0, NULL );

    SelectObject( hdc, old_font );
}

static void get_region_results( HRGN hrgn, int ret, struct region_results *res )
{
    res->ret = ret;
    if (ret > 0) res->size = GetRegionData( hrgn, sizeof(res->data), (RGNDATA *)res->data );
}

static void measure_path( HDC hdc, const char *text, struct path_results *res )
{
    HRGN rgn;

    BeginPath( hdc );
    MoveToEx( hdc, 5, 7, NULL );
    if (text) TextOutA( hdc, 9, 11, text, strlen( text ) );
    LineTo( hdc, 170, 120 );
    LineTo( hdc, 30, 130 );
    CloseFigure( hdc );
    EndPath( hdc );
    res->count = GetPath( hdc, NULL, NULL, 0 );
    if (res->count > 0 && res->count <= ARRAY_SIZE(res->points))
        GetPath( hdc, res->points, res->types, res->count );
    rgn = PathToRegion( hdc );
    res->rgn_ret = GetRgnBox( rgn, &res->rgn_box );
    DeleteObject( rgn );
}

static void measure_dc( HDC hdc, HWND hwnd, const HFONT *fonts, struct dc_results *res )
{
    static const int grid_x[5] = { -37, 0, 13, 101, 255 }, grid_y[5] = { -23, 0, 7, 77, 149 };
    static const int vis_x[8] = { 0, 12, 13, 99, 150, 151, 199, 200 };
    static const int vis_y[8] = { 0, 6, 7, 55, 96, 97, 149, 150 };
    static const XFORM xform = { 1.5f, 0.0f, 0.0f, 1.5f, 0.0f, 0.0f };
    RECT rect, scroll_rect = { 10, 10, 110, 90 }, scroll_clip = { 0, 0, 150, 120 };
    POINT org;
    HRGN rgn;
    int i, j;

    rgn = CreateRectRgn( 0, 0, 0, 0 );

    res->caps[0] = GetDeviceCaps( hdc, LOGPIXELSX );
    res->caps[1] = GetDeviceCaps( hdc, LOGPIXELSY );
    res->caps[2] = GetDeviceCaps( hdc, HORZRES );
    res->caps[3] = GetDeviceCaps( hdc, VERTRES );

    /* MM_TEXT, MM_LOMETRIC, MM_ANISOTROPIC, world transform, RTL layout */
    for (i = 0; i < ARRAY_SIZE(res->lp_to_dp); i++)
    {
        SaveDC( hdc );
        switch (i)
        {
        case 1:
            SetMapMode( hdc, MM_LOMETRIC );
            break;
        case 2:
            SetMapMode( hdc, MM_ANISOTROPIC );
            SetWindowExtEx( hdc, 3, 7, NULL );
            SetViewportExtEx( hdc, 7, 3, NULL );
            break;
        case 3:
            SetGraphicsMode( hdc, GM_ADVANCED );
            SetWorldTransform( hdc, &xform );
            break;
        case 4:
            SetLayout( hdc, LAYOUT_RTL );
            break;
        }
        for (j = 0; j < 25; j++)
        {
            res->lp_to_dp[i][j].x = res->dp_to_lp[i][j].x = grid_x[j % 5];
            res->lp_to_dp[i][j].y = res->dp_to_lp[i][j].y = grid_y[j / 5];
        }
        LPtoDP( hdc, res->lp_to_dp[i], 25 );
        DPtoLP( hdc, res->dp_to_lp[i], 25 );
        SetLayout( hdc, 0 );
        RestoreDC( hdc, -1 );
    }

    SaveDC( hdc );
    res->clip_ret[0] = GetClipBox( hdc, &res->clip_box[0] );
    IntersectClipRect( hdc, 3, 5, 177, 131 );
    res->clip_ret[1] = GetClipBox( hdc, &res->clip_box[1] );
    ExcludeClipRect( hdc, 21, 19, 47, 33 );
    res->clip_ret[2] = GetClipBox( hdc, &res->clip_box[2] );
    SetRectRgn( rgn, 11, 13, 163, 121 );
    ExtSelectClipRgn( hdc, rgn, RGN_AND );
    res->clip_ret[3] = GetClipBox( hdc, &res->clip_box[3] );
    SetRectRgn( rgn, 31, 41, 59, 67 );
    ExtSelectClipRgn( hdc, rgn, RGN_DIFF );
    res->clip_ret[4] = GetClipBox( hdc, &res->clip_box[4] );
    get_region_results( rgn, GetClipRgn( hdc, rgn ), &res->clip_rgn[0] );
    SaveDC( hdc );
    IntersectClipRect( hdc, 1, 1, 99, 77 );
    res->clip_ret[5] = GetClipBox( hdc, &res->clip_box[5] );
    RestoreDC( hdc, -1 );
    res->clip_ret[6] = GetClipBox( hdc, &res->clip_box[6] );
    SetMetaRgn( hdc );
    res->clip_ret[7] = GetClipBox( hdc, &res->clip_box[7] );
    get_region_results( rgn, GetClipRgn( hdc, rgn ), &res->clip_rgn[1] );
    get_region_results( rgn, GetMetaRgn( hdc, rgn ), &res->meta_rgn );
    RestoreDC( hdc, -1 );

    res->sysrgn_ret = GetRandomRgn( hdc, rgn, SYSRGN );
    GetRgnBox( rgn, &res->sysrgn_box );

    GetDCOrgEx( hdc, &org );
    GetWindowRect( hwnd, &rect );
    res->dc_org.x = org.x - rect.left;
    res->dc_org.y = org.y - rect.top;

    for (i = 0; i < ARRAY_SIZE(res->pt_visible); i++)
    {
        int x = vis_x[i % 8], y = vis_y[i / 8];

        res->pt_visible[i] = PtVisible( hdc, x, y );
        SetRect( &rect, x - 1, y - 1, x + 1, y + 1 );
        res->rect_visible[i] = RectVisible( hdc, &rect );
    }

    SaveDC( hdc );
    MoveToEx( hdc, 17, 23, NULL );
    LineTo( hdc, 61, 45 );
    GetCurrentPositionEx( hdc, &res->line_pos );
    SetTextAlign( hdc, TA_UPDATECP );
    TextOutA( hdc, 0, 0, "Wine", 4 );
    GetCurrentPositionEx( hdc, &res->text_pos );
    RestoreDC( hdc, -1 );

    SaveDC( hdc );
    SelectObject( hdc, fonts[2] );
    measure_path( hdc, NULL, &res->path[0] );
    measure_path( hdc, "Ab", &res->path[1] );
    RestoreDC( hdc, -1 );

    SetRectRgn( rgn, 0, 0, 0, 0 );
    res->scroll_ret = ScrollDC( hdc, 7, -5, &scroll_rect, &scroll_clip, rgn, &res->scroll_rect );
    res->scroll_rgn_ret = GetRgnBox( rgn, &res->scroll_rgn_box );

    for (i = 0; i < ARRAY_SIZE(res->text); i++) measure_text( hdc, fonts[i], &res->text[i] );

    DeleteObject( rgn );
}

static void check_dc_pixels( HDC hdc )
{
    static const COLORREF color = RGB( 0xff, 0x00, 0xff ), fill = RGB( 0x00, 0xff, 0x00 );
    BITMAPINFO info = {{ sizeof(BITMAPINFOHEADER), 64, -48, 1, 32, BI_RGB }};
    RECT rect = { 20, 10, 84, 58 };
    HBITMAP bitmap;
    COLORREF pixel;
    HBRUSH brush;
    DWORD *bits;
    HDC mem_dc;
    BITMAP bm;
    int i;

    bitmap = CreateCompatibleBitmap( hdc, 37, 23 );
    ok( bitmap != 0, "CreateCompatibleBitmap failed\n" );
    memset( &bm, 0, sizeof(bm) );
    GetObjectA( bitmap, sizeof(bm), &bm );
    ok( bm.bmWidth == 37 && bm.bmHeight == 23, "got bitmap size %dx%d\n", bm.bmWidth, bm.bmHeight );
    DeleteObject( bitmap );

    if (!test_unverified) return;

    SetPixel( hdc, 30, 20, color );
    pixel = GetPixel( hdc, 30, 20 );
    ok( pixel == color, "got pixel %08lx\n", pixel );

    brush = CreateSolidBrush( fill );
    FillRect( hdc, &rect, brush );
    DeleteObject( brush );

    mem_dc = CreateCompatibleDC( hdc );
    bitmap = CreateDIBSection( mem_dc, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0 );
    ok( bitmap != 0, "CreateDIBSection failed\n" );
    SelectObject( mem_dc, bitmap );
    BitBlt( mem_dc, 0, 0, 64, 48, hdc, rect.left, rect.top, SRCCOPY );
    GdiFlush();
    for (i = 0; i < 64 * 48; i++) if (bits[i] != 0x00ff00) break;
    ok( i == 64 * 48, "got %08lx at %d,%d\n", i < 64 * 48 ? bits[i] : 0, i % 64, i / 64 );
    DeleteDC( mem_dc );
    DeleteObject( bitmap );
}

static void measure_window( DPI_AWARENESS_CONTEXT context, const HFONT *fonts, struct dc_results *res )
{
    static const DWORD style = WS_POPUP | WS_BORDER | WS_VISIBLE, ex_style = WS_EX_TOPMOST;
    RECT rect = { 13, 7, 151, 97 }, window_rect = { 50, 50, 250, 200 };
    PAINTSTRUCT ps;
    HWND hwnd;
    HRGN rgn;
    HDC hdc;
    int i;

    pSetThreadDpiAwarenessContext( context );
    /* use a frame so that window DCs have a different origin than client DCs */
    AdjustWindowRectEx( &window_rect, style, FALSE, ex_style );
    hwnd = CreateWindowExA( ex_style, "cache_class", NULL, style, window_rect.left, window_rect.top,
                            window_rect.right - window_rect.left, window_rect.bottom - window_rect.top,
                            0, 0, GetModuleHandleA(0), NULL );
    ok( hwnd != 0, "failed to create window\n" );
    UpdateWindow( hwnd );
    flush_events();

    rgn = CreateRectRgn( 0, 0, 0, 0 );
    for (i = 0; i < ARRAY_SIZE(dc_names); i++)
    {
        winetest_push_context( "%s", dc_names[i] );
        if (i == 0) hdc = GetDC( hwnd );
        else if (i == 1) hdc = GetDCEx( hwnd, 0, DCX_WINDOW | DCX_CACHE );
        else
        {
            RedrawWindow( hwnd, NULL, 0, RDW_VALIDATE | RDW_NOFRAME | RDW_NOERASE );
            InvalidateRect( hwnd, &rect, FALSE );
            res[i].update_ret = GetUpdateRect( hwnd, &res[i].update_rect, FALSE );
            get_region_results( rgn, GetUpdateRgn( hwnd, rgn, FALSE ), &res[i].update_rgn );
            hdc = BeginPaint( hwnd, &ps );
            res[i].paint_rect = ps.rcPaint;
        }
        ok( hdc != 0, "failed to get DC\n" );

        measure_dc( hdc, hwnd, fonts, &res[i] );
        check_dc_pixels( hdc );

        if (i == 2) EndPaint( hwnd, &ps );
        else ReleaseDC( hwnd, hdc );
        winetest_pop_context();
    }
    DeleteObject( rgn );

    DestroyWindow( hwnd );
}

static void compare_text_results( const struct text_results *res, const struct text_results *expect )
{
    check_results( res, expect, tm_ret );
    check_results( res, expect, tm );
    check_results( res, expect, extent );
    check_results( res, expect, fit );
    check_results( res, expect, dx );
    check_results( res, expect, width_ret );
    check_results( res, expect, width );
    check_results( res, expect, abc_ret );
    check_results( res, expect, abc );
    check_results( res, expect, otm );
    check_results( res, expect, glyph_ret );
    check_results( res, expect, gm );
    check_results( res, expect, kern_pairs );
}

static void compare_dc_results( const struct dc_results *res, const struct dc_results *expect )
{
    check_results( res, expect, update_ret );
    check_results( res, expect, update_rect );
    check_results( res, expect, update_rgn );
    check_results( res, expect, paint_rect );
    check_results( res, expect, caps );
    check_results( res, expect, lp_to_dp );
    check_results( res, expect, dp_to_lp );
    check_results( res, expect, clip_ret );
    check_results( res, expect, clip_box );
    check_results( res, expect, clip_rgn );
    check_results( res, expect, meta_rgn );
    check_results( res, expect, dc_org );
    check_results( res, expect, pt_visible );
    check_results( res, expect, rect_visible );
    check_results( res, expect, line_pos );
    check_results( res, expect, path[0].count );
    check_results( res, expect, path[0].points );
    check_results( res, expect, path[0].types );
    check_results( res, expect, path[0].rgn_ret );
    check_results( res, expect, path[0].rgn_box );
    check_results( res, expect, scroll_ret );
    check_results( res, expect, scroll_rect );
    check_results( res, expect, scroll_rgn_ret );
    check_results( res, expect, scroll_rgn_box );
    if (test_unverified)
    {
        check_results( res, expect, sysrgn_ret );
        check_results( res, expect, sysrgn_box );
        /* these depend on text metrics and glyph outlines */
        check_results( res, expect, text_pos );
        check_results( res, expect, path[1].count );
        check_results( res, expect, path[1].points );
        check_results( res, expect, path[1].types );
        check_results( res, expect, path[1].rgn_ret );
        check_results( res, expect, path[1].rgn_box );
    }
}

/* GDI scaling must not change anything an application can observe through GDI */
static void test_gdi_scaled_dc(void)
{
    struct dc_results *unaware, *scaled;
    struct text_results *compat;
    DPI_AWARENESS_CONTEXT orig;
    HFONT fonts[5];
    LOGFONTA lf;
    UINT system_dpi;
    HDC hdc;
    int i, j;

    if (!pSetThreadDpiAwarenessContext ||
        !(orig = pSetThreadDpiAwarenessContext( DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED )))
    {
        win_skip( "DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED not supported\n" );
        return;
    }

    /* GetDpiForSystem() always returns 96 in DPI unaware threads */
    pSetThreadDpiAwarenessContext( DPI_AWARENESS_CONTEXT_SYSTEM_AWARE );
    system_dpi = pGetDpiForSystem();
    test_unverified = winetest_platform_is_wine || system_dpi == USER_DEFAULT_SCREEN_DPI;
    if (!test_unverified) skip( "system DPI %u, skipping some tests\n", system_dpi );

    fonts[0] = GetStockObject( SYSTEM_FONT );
    memset( &lf, 0, sizeof(lf) );
    lf.lfHeight = -11;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    strcpy( lf.lfFaceName, "MS Shell Dlg" );
    fonts[1] = CreateFontIndirectA( &lf );
    lf.lfHeight = -13;
    strcpy( lf.lfFaceName, "Tahoma" );
    fonts[2] = CreateFontIndirectA( &lf );
    lf.lfHeight = 0;
    fonts[3] = CreateFontIndirectA( &lf );
    lf.lfHeight = -13;
    lf.lfWeight = FW_BOLD;
    fonts[4] = CreateFontIndirectA( &lf );

    unaware = calloc( ARRAY_SIZE(dc_names), sizeof(*unaware) );
    scaled = calloc( ARRAY_SIZE(dc_names), sizeof(*scaled) );
    compat = calloc( ARRAY_SIZE(fonts), sizeof(*compat) );

    winetest_push_context( "unaware" );
    measure_window( DPI_AWARENESS_CONTEXT_UNAWARE, fonts, unaware );
    winetest_pop_context();
    winetest_push_context( "gdiscaled" );
    measure_window( DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED, fonts, scaled );
    winetest_pop_context();

    pSetThreadDpiAwarenessContext( DPI_AWARENESS_CONTEXT_UNAWARE );
    hdc = CreateCompatibleDC( 0 );
    for (i = 0; i < ARRAY_SIZE(fonts); i++) measure_text( hdc, fonts[i], &compat[i] );
    DeleteDC( hdc );

    for (i = 0; i < ARRAY_SIZE(dc_names); i++)
    {
        winetest_push_context( "%s", dc_names[i] );
        compare_dc_results( &scaled[i], &unaware[i] );
        for (j = 0; test_unverified && j < ARRAY_SIZE(fonts); j++)
        {
            winetest_push_context( "%s unaware", font_names[j] );
            compare_text_results( &scaled[i].text[j], &unaware[i].text[j] );
            winetest_pop_context();
            winetest_push_context( "%s memory DC", font_names[j] );
            compare_text_results( &scaled[i].text[j], &compat[j] );
            winetest_pop_context();
        }
        winetest_pop_context();
    }

    free( compat );
    free( scaled );
    free( unaware );
    for (i = 1; i < ARRAY_SIZE(fonts); i++) DeleteObject( fonts[i] );
    pSetThreadDpiAwarenessContext( orig );
}

static void test_destroyed_window(void)
{
    HDC dc, old_dc;
    int rop;

    dc = GetDC( hwnd_cache );
    SetROP2( dc, R2_WHITE );
    rop = GetROP2( dc );
    ok( rop == R2_WHITE, "wrong ROP2 %d\n", rop );
    ok( WindowFromDC( dc ) == hwnd_cache, "wrong window\n" );
    old_dc = dc;

    DestroyWindow( hwnd_cache );
    rop = GetROP2( dc );
    ok( rop == 0, "wrong ROP2 %d\n", rop );
    ok( WindowFromDC( dc ) != hwnd_cache, "wrong window\n" );
    ok( !ReleaseDC( hwnd_cache, dc ), "ReleaseDC succeeded\n" );
    dc = GetDC( hwnd_cache );
    ok( !dc, "Got a non-NULL DC (%p) for a destroyed window\n", dc );

    dc = GetDC( hwnd_classdc );
    SetROP2( dc, R2_WHITE );
    rop = GetROP2( dc );
    ok( rop == R2_WHITE, "wrong ROP2 %d\n", rop );
    ok( WindowFromDC( dc ) == hwnd_classdc, "wrong window\n" );
    old_dc = dc;

    dc = GetDC( hwnd_classdc2 );
    ok( old_dc == dc, "wrong DC\n" );
    rop = GetROP2( dc );
    ok( rop == R2_WHITE, "wrong ROP2 %d\n", rop );
    ok( WindowFromDC( dc ) == hwnd_classdc2, "wrong window\n" );
    DestroyWindow( hwnd_classdc2 );

    rop = GetROP2( dc );
    ok( rop == R2_WHITE, "wrong ROP2 %d\n", rop );
    ok( WindowFromDC( dc ) != hwnd_classdc2, "wrong window\n" );
    ok( !ReleaseDC( hwnd_classdc2, dc ), "ReleaseDC succeeded\n" );
    dc = GetDC( hwnd_classdc2 );
    ok( !dc, "Got a non-NULL DC (%p) for a destroyed window\n", dc );

    dc = GetDC( hwnd_classdc );
    ok( dc != 0, "Got NULL DC\n" );
    rop = GetROP2( dc );
    ok( rop == R2_WHITE, "wrong ROP2 %d\n", rop );
    ok( WindowFromDC( dc ) == hwnd_classdc, "wrong window\n" );
    DestroyWindow( hwnd_classdc );

    rop = GetROP2( dc );
    ok( rop == R2_WHITE, "wrong ROP2 %d\n", rop );
    ok( WindowFromDC( dc ) != hwnd_classdc, "wrong window\n" );
    ok( !ReleaseDC( hwnd_classdc, dc ), "ReleaseDC succeeded\n" );
    dc = GetDC( hwnd_classdc );
    ok( !dc, "Got a non-NULL DC (%p) for a destroyed window\n", dc );

    dc = GetDC( hwnd_owndc );
    ok( dc != 0, "Got NULL DC\n" );
    rop = GetROP2( dc );
    ok( rop == R2_WHITE, "wrong ROP2 %d\n", rop );
    ok( WindowFromDC( dc ) == hwnd_owndc, "wrong window\n" );
    DestroyWindow( hwnd_owndc );

    rop = GetROP2( dc );
    ok( rop == 0, "wrong ROP2 %d\n", rop );
    ok( WindowFromDC( dc ) != hwnd_owndc, "wrong window\n" );
    ok( !ReleaseDC( hwnd_owndc, dc ), "ReleaseDC succeeded\n" );
    dc = GetDC( hwnd_owndc );
    ok( !dc, "Got a non-NULL DC (%p) for a destroyed window\n", dc );

    DestroyWindow( hwnd_parent );
}

START_TEST(dce)
{
    HMODULE user32 = GetModuleHandleA( "user32.dll" );
    WNDCLASSA cls;

    pSetThreadDpiAwarenessContext = (void *)GetProcAddress( user32, "SetThreadDpiAwarenessContext" );
    pGetDpiForSystem = (void *)GetProcAddress( user32, "GetDpiForSystem" );

    cls.style = CS_DBLCLKS;
    cls.lpfnWndProc = DefWindowProcA;
    cls.cbClsExtra = 0;
    cls.cbWndExtra = 0;
    cls.hInstance = GetModuleHandleA(0);
    cls.hIcon = 0;
    cls.hCursor = LoadCursorA(0, (LPCSTR)IDC_ARROW);
    cls.hbrBackground = GetStockObject(WHITE_BRUSH);
    cls.lpszMenuName = NULL;
    cls.lpszClassName = "cache_class";
    RegisterClassA(&cls);
    cls.style = CS_DBLCLKS | CS_OWNDC;
    cls.lpszClassName = "owndc_class";
    RegisterClassA(&cls);
    cls.style = CS_DBLCLKS | CS_CLASSDC;
    cls.lpszClassName = "classdc_class";
    RegisterClassA(&cls);
    cls.style = CS_PARENTDC;
    cls.lpszClassName = "parentdc_class";
    RegisterClassA(&cls);

    hwnd_cache = CreateWindowA("cache_class", NULL, WS_OVERLAPPED | WS_VISIBLE,
                               0, 0, 100, 100,
                               0, 0, GetModuleHandleA(0), NULL );
    hwnd_owndc = CreateWindowA("owndc_class", NULL, WS_OVERLAPPED | WS_VISIBLE,
                               0, 200, 100, 100,
                               0, 0, GetModuleHandleA(0), NULL );
    hwnd_classdc = CreateWindowA("classdc_class", NULL, WS_OVERLAPPED | WS_VISIBLE,
                                 200, 0, 100, 100,
                                 0, 0, GetModuleHandleA(0), NULL );
    hwnd_classdc2 = CreateWindowA("classdc_class", NULL, WS_OVERLAPPED | WS_VISIBLE,
                                  200, 200, 100, 100,
                                  0, 0, GetModuleHandleA(0), NULL );
    hwnd_parent = CreateWindowA("static", NULL, WS_OVERLAPPED | WS_VISIBLE,
                                400, 0, 100, 100, 0, 0, 0, NULL );
    hwnd_parentdc = CreateWindowA("parentdc_class", NULL, WS_CHILD | WS_VISIBLE,
                                  50, 50, 1, 1, hwnd_parent, 0, 0, NULL );

    test_dc_attributes();
    test_parameters();
    test_dc_visrgn();
    test_begin_paint();
    test_cropped_parentdc_paint_clipbox();
    test_scroll_window();
    test_invisible_create();
    test_dc_layout();
    test_gdi_scaled_dc();
    /* this should be last */
    test_destroyed_window();
}
