/* -------------------- DisplayManager.cpp -------------------- */
#include "DisplayManager.h"
#include "ST7365P_Display.h"
#include "MenuState.h"
#include "InterlockManager.h"
#include "AuxManager.h"

/* === single global display instance (visible to all .cpps) === */
ST7365P_Display tft;

/* ─────────── internal helpers ─────────── */
static void paintTab(TabID tab, bool selected);
static void paintOverviewItem(uint8_t idx, bool selected);
static void paintDummyItem(uint8_t idx, bool selected);
void        redrawAuxRow(uint8_t idx);   // from AuxManager.cpp

/* remember what is currently shown */
static uint8_t lastTab  = 0;
static int8_t  lastItem = NO_SELECTION;

/* ═════════════════  TAB HEADER  ═════════════════ */
static void paintTab(TabID tab, bool sel)
{
    const uint16_t x = 10 + tab * 160;
    tft.fillRect(x, 0, 150, 24, sel ? COLOR_SELECTED_BG : COLOR_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(sel ? COLOR_YELLOW : COLOR_WHITE);
    tft.setCursor(x, 4);
    switch (tab) {
        case TAB_OVERVIEW:  tft.print("Overview");  break;
        case TAB_SETTINGS:  tft.print("Settings");  break;
        case TAB_AUXILIARY: tft.print("Aux");       break;
        default: break;
    }
}

/* ═════════════════  OVERVIEW ROW  ═════════════════ */
static void drawStatusCircle(int16_t x,int16_t y,uint16_t col,bool outline)
{
    if (outline){
        tft.fillCircle(x,y,10,COLOR_YELLOW);
        tft.fillCircle(x,y, 8,col);
    } else tft.fillCircle(x,y,8,col);
}

static void paintOverviewItem(uint8_t i,bool sel)
{
    const uint16_t y = 30 + i*24;
    const auto &it  = interlocks[i];

    /* row background */
    tft.fillRect(0,y,480,24, sel?COLOR_SELECTED_BG:COLOR_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(sel?COLOR_YELLOW:COLOR_WHITE);
    tft.setCursor(2,y+6);
    tft.print(it.label);

    /* status circle */
    bool outline=false;
    uint16_t col;

    if (menuState.editMode && i==menuState.selectedItem){
        /* preview during edit */
        switch(menuState.editStateIndex){
            case 0: col = COLOR_RED;   outline=false; break;
            case 1: col = COLOR_RED;   outline=true;  break;
            case 2: col = COLOR_GREEN; outline=true;  break;
        }
    } else {
        /* live state */
        bool sim = isSimulated(it.port,it.bit);
        if (sim){
            bool o = (readOutputRegister(it.port)>>it.bit)&1;
            col = o?COLOR_RED:COLOR_GREEN; outline=true;
        } else {
            bool v = readInterlock(it.port,it.bit);
            col = v?COLOR_GREEN:COLOR_RED; outline=false;
        }
    }
    drawStatusCircle(460,y+12,col,outline);
}

/* ═════════════════  DUMMY ROWS (SETTINGS)  ═════════════════ */
static void paintDummyItem(uint8_t idx,bool sel)
{
    const uint16_t y = 30 + idx*24;
    tft.fillRect(0,y,480,24, sel?COLOR_SELECTED_BG:COLOR_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(sel?COLOR_YELLOW:COLOR_WHITE);
    tft.setCursor(2,y+6);
    tft.print("Item ");
    tft.print(idx+1);
}

/* ═════════════════  PUBLIC API  ═════════════════ */
void redrawAll()
{
    /* header */
    tft.fillRect(0,0,480,24,COLOR_BLACK);
    for(uint8_t i=0;i<TAB_COUNT;++i) paintTab((TabID)i,i==menuState.currentTab);

    /* body */
    tft.fillRect(0,30,480,242,COLOR_BLACK);
    uint8_t n = itemCountForTab(menuState.currentTab);
    for(uint8_t i=0;i<n;++i){
        switch(menuState.currentTab){
            case TAB_OVERVIEW:  paintOverviewItem(i,false); break;
            case TAB_SETTINGS:  paintDummyItem(i,false);    break;
            case TAB_AUXILIARY: redrawAuxRow(i);            break;
            default: break;
        }
    }
    updateEditIndicator(menuState.editMode);

    lastTab  = menuState.currentTab;
    lastItem = menuState.selectedItem;
}

void updateTab()
{
    if (menuState.currentTab == lastTab) return;

    /* repaint headers */
    paintTab((TabID)lastTab,false);
    paintTab(menuState.currentTab,true);

    /* clear body & draw new tab */
    tft.fillRect(0,30,480,242,COLOR_BLACK);
    uint8_t n = itemCountForTab(menuState.currentTab);
    for(uint8_t i=0;i<n;++i){
        switch(menuState.currentTab){
            case TAB_OVERVIEW:  paintOverviewItem(i,false); break;
            case TAB_SETTINGS:  paintDummyItem(i,false);    break;
            case TAB_AUXILIARY: redrawAuxRow(i);            break;
            default: break;
        }
    }
    updateEditIndicator(menuState.editMode);

    lastTab  = menuState.currentTab;
    lastItem = menuState.selectedItem;
}

void updateItem()
{
    /* deselect previous */
    if (lastItem != NO_SELECTION){
        switch(menuState.currentTab){
            case TAB_OVERVIEW:  paintOverviewItem(lastItem,false); break;
            case TAB_SETTINGS:  paintDummyItem   (lastItem,false); break;
            case TAB_AUXILIARY: redrawAuxRow     (lastItem);       break;
            default: break;
        }
    }

    /* select new   */
    if (menuState.selectedItem != NO_SELECTION){
        switch(menuState.currentTab){
            case TAB_OVERVIEW:  paintOverviewItem(menuState.selectedItem,true); break;
            case TAB_SETTINGS:  paintDummyItem   (menuState.selectedItem,true); break;
            case TAB_AUXILIARY: redrawAuxRow     (menuState.selectedItem);      break;
            default: break;
        }
    }
    lastItem = menuState.selectedItem;
}

void updateEditIndicator(bool on)
{
    tft.fillRect(460,0,20,20,on?COLOR_RED:COLOR_BLACK);
    if (on){
        tft.setCursor(462,4);
        tft.setTextColor(COLOR_BLACK);
        tft.print('E');
    }
}

void flashResetIndicator()
{
    const uint16_t y = 30 + 8*24;
    paintOverviewItem(8,true);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_YELLOW,COLOR_SELECTED_BG);
    tft.setCursor(420,y+6);
    tft.print('*');
    delay(300);
    paintOverviewItem(8,true);
}

void showIdleScreen()
{
    menuState.screen = SCREEN_IDLE;
    tft.fillScreen(COLOR_BLACK);
    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(2);
    tft.setCursor(60,120);
    tft.print("European Spallation Source");
}

void initDisplay()
{
    tft.begin();
    tft.setRotation(2);
    tft.setTextSize(2);
    tft.fillScreen(COLOR_BLACK);
    redrawAll();
}

void paintItem(uint8_t idx, bool selected)
{
    switch (menuState.currentTab)
    {
        case TAB_OVERVIEW:  paintOverviewItem(idx, selected); break;
        case TAB_SETTINGS:  paintDummyItem   (idx, selected); break;
        case TAB_AUXILIARY: redrawAuxRow     (idx);           break;
        default: break;
    }
}