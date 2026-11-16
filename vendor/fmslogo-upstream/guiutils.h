// -*- c++ -*-
#include <stddef.h>

class wxWindow;
class wxMenu;
class wxMenuBar;
class wxTopLevelWindow;

struct MENUITEM
{
    const wchar_t * MenuText;
    int             MenuId;
};

struct ACCELERATORITEM
{
    int           Flags;
    int           KeyCode;
    int           CommandId;
};

void
SetFmsLogoIcon(
    wxTopLevelWindow & TopLevelWindow
    );

void
SetAccelerators(
    wxWindow *              Window,
    const ACCELERATORITEM * AcceleratorItems,
    size_t                  AcceleratorItemsLength
    );

void
FillMenu(
    wxMenu *          Menu,
    const MENUITEM *  MenuItems,
    size_t            MenuItemsLength
    );

void
AppendChildMenu(
    wxMenuBar *       MainMenu,
    const wchar_t *   ChildMenuText,
    const MENUITEM *  ChildMenuItems,
    size_t            ChildMenuItemsLength
    );
