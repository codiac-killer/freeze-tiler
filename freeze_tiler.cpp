#define NOMINMAX
#include <iostream>
#include <string>
#include <limits>
#include <windows.h>
#include <synchapi.h>
#include <shobjidl.h>
#include <atlstr.h>
#include <thread>
#include <chrono>
#include <cmath>
#include <vector>
#include <algorithm>

BOOL com_init = SUCCEEDED(CoInitialize(NULL));

IVirtualDesktopManager* g_pvdm;

struct MonitorRects
{
    std::vector<RECT>   rcMonitors;
    RECT                rcCombined;

    static BOOL CALLBACK MonitorEnum(HMONITOR hMon,HDC hdc,LPRECT lprcMonitor,LPARAM pData)
    {
        MonitorRects* pThis = reinterpret_cast<MonitorRects*>(pData);
        pThis->rcMonitors.push_back(*lprcMonitor);
        UnionRect(&pThis->rcCombined, &pThis->rcCombined, lprcMonitor);
        return TRUE;
    }

    MonitorRects()
    {
        SetRectEmpty(&rcCombined);
        EnumDisplayMonitors(0, 0, MonitorEnum, (LPARAM)this);
    }
};

bool compareRect(RECT rect1, RECT rect2){
   return rect1.left < rect2.left;
}


static BOOL CALLBACK window_enumeration_handler(HWND hwnd, LPARAM parameters){
    // Get Window's rectangle
    RECT window_rect; 
    GetWindowRect(hwnd, &window_rect);
    // Calculate window size
    bool win_size = (window_rect.right - window_rect.left) * (window_rect.bottom - window_rect.top) != 0;

    // Filter system windows by name
    int length = GetWindowTextLength(hwnd);
    char* win_text = new char[length + 1];
# define _CRT_SECURE_NO_WARNINGS
    GetWindowTextA(hwnd, win_text, length + 1);
    std::string title(win_text);
    bool title_flag = title != "" && title != "Program Manager" && title != "Calculator" && title != "Settings" &&
        title != "Microsoft Store" && title != "Movies & TV" && title != "Microsoft Text Input Application" &&
        title != "Task Manager" && title != "Photos" && title != "Cortana" && title != "Microsoft Edge";

    // Filter by style
    long style = GetWindowLong(hwnd, GWL_STYLE);
    long extended_style = GetWindowLong(hwnd, GWL_EXSTYLE);
    // TODO make sure second filter doesn't cause problem (need for some popups)
    bool is_not_toolbar = (!(extended_style & WS_EX_TOOLWINDOW)) && (!(style & WS_EX_TOOLWINDOW));
    bool is_not_popup = !(style & WS_POPUP);
    bool is_not_controlling_parent = !(extended_style & WS_EX_CONTROLPARENT);
    bool is_not_maximised = !(style & WS_MAXIMIZE);

    BOOL is_on_current_desktop = FALSE;
        
    if(com_init) g_pvdm->IsWindowOnCurrentVirtualDesktop(hwnd, &is_on_current_desktop);

    // Added window to list if conditions met
    if(!IsIconic(hwnd) && IsWindow(hwnd) && IsWindowVisible(hwnd) && win_size &&
       is_not_toolbar && is_not_controlling_parent && is_not_maximised && is_not_popup
       && is_on_current_desktop && title_flag){

        std::vector<HWND> &window_list = *reinterpret_cast<std::vector<HWND>*>(parameters);
        window_list.emplace_back(hwnd);

    }

    return true;
}


std::vector<std::vector<HWND>> get_non_minimized_windows(std::vector<RECT> monitor_parameters){
    // Get Non Minimized windows
    std::vector<HWND> top_windows;
    EnumWindows(window_enumeration_handler, reinterpret_cast<LPARAM>(&top_windows));

    // Determine regions of screens
    std::vector<std::vector<HWND>> windows_p_screen(monitor_parameters.size());

    // Divide window list by screen
    for ( const auto& win : top_windows ){
        // Get Window's rectangle
        RECT window_rect; 
        GetWindowRect(win, &window_rect);

        for(int i = 0; i < monitor_parameters.size(); i++){
            // IF window is lefter than the left screen
            if(i == 0 && window_rect.left < monitor_parameters[i].left){
                windows_p_screen[i].emplace_back(win);
            // IF window starts somewhere in the screen
            } else if((monitor_parameters[i].left - 8) <= window_rect.left && window_rect.left < monitor_parameters[i].right){
                windows_p_screen[i].emplace_back(win);
            // If window is righter than the right screen (probably wont happen)
            } else if(i == monitor_parameters.size() - 1 && window_rect.left > monitor_parameters[i].right){
                windows_p_screen[i].emplace_back(win);
            }
        }
    }

    // return window list divided to screens
    return windows_p_screen;
}


void tile_windows(std::vector<RECT> monitor_parameters, int WINDOW_GAP, int SCREEN_GAP){
  // Find all visible windows
  auto visible_windows = get_non_minimized_windows(monitor_parameters);

  int monitor_count = monitor_parameters.size();
  for(int i = 0; i < monitor_count; i++){
    // Check if desktop is empty ----------------------------
    if(visible_windows[i].empty()) continue;
    // ------------------------------------------------------

    // Calculate Grid ---------------------------------------
    // Get screen dimensions
    auto screen_wid = monitor_parameters[i].right - monitor_parameters[i].left;
    auto screen_height = monitor_parameters[i].bottom - monitor_parameters[i].top;

    // Get working area
    auto grid_wid = screen_wid - 2*SCREEN_GAP;
    auto grid_height = screen_height - 2*SCREEN_GAP;
    // Initialize grid (1 tile)
    std::vector<int> first_tile = {monitor_parameters[i].left + SCREEN_GAP,
                                   monitor_parameters[i].top + SCREEN_GAP,
                                   monitor_parameters[i].left + SCREEN_GAP + grid_wid,
                                   monitor_parameters[i].top + SCREEN_GAP + grid_height - 40 // -40 because of taskbar
                                  };
    std::vector<std::vector<int>> grid = {first_tile};

    // Number of tiles
    int num_of_tiles = visible_windows[i].size();

    // Calculate tile boxes
    int tile_index = 1;  // which tile gets halved for next step
    int power_of_tiles = 0;  // log2(i)

    for(int tile = 1; tile < num_of_tiles; tile++){
      // slice tile in half if power_of_tiles even vertically else horizontally
      if(power_of_tiles % 2 == 0){  // split vertically
        // pop old tile
        auto old_tile = grid[grid.size() - tile_index];
        grid.erase(grid.end() - tile_index);

        std::vector<int> new_tile = {old_tile[0], old_tile[1], int((old_tile[0] + old_tile[2])/2 - WINDOW_GAP/2), old_tile[3]};
        auto index = (tile_index == grid.size() + 1) ? grid.begin() : grid.end() - tile_index + 1;

        // replace with two new tiles
        grid.emplace(
            index,
            new_tile
        );

        auto new_tile2 = {int((old_tile[0] + old_tile[2])/2 + WINDOW_GAP/2), old_tile[1], old_tile[2], old_tile[3]};
        index = (tile_index == grid.size() + 1) ? grid.begin() : grid.end() - tile_index + 1;

        grid.emplace(
            index,
            new_tile2
        );

      } else {  // split horizontally
        // pop old tile
        auto old_tile = grid[grid.size() - tile_index];
        grid.erase(grid.end() - tile_index);

        auto new_tile = {old_tile[0], old_tile[1], old_tile[2], int((old_tile[1] + old_tile[3]) / 2 - WINDOW_GAP / 2)};
        auto index = (tile_index == grid.size() + 1 ) ? grid.begin() : grid.end() - tile_index + 1;

        // replace with two new tiles
        grid.emplace(
            index,
            new_tile
        );

        auto new_tile2 = {old_tile[0], int((old_tile[1] + old_tile[3]) / 2 + WINDOW_GAP / 2), old_tile[2], old_tile[3]};
        index = (tile_index == grid.size() + 1) ? grid.begin() : grid.end() - tile_index + 1;

        grid.emplace(
            index,
            new_tile2
        );
      }

        // Check if log2(tile) is integer
        if(ceilf(log2(tile + 1)) == log2(tile + 1)){
          power_of_tiles++;
          // Reset tile_index
          tile_index = 1;
        } else {
          // Increase counter
          tile_index += 2;
        }
    }
    // ------------------------------------------------------

    // Place focused window to tile if it is inside one -----
    // Get Rectangle of focused window
    auto window = GetForegroundWindow();
    auto focused_window = std::find(visible_windows[i].begin(), visible_windows[i].end(), window);
    if(focused_window != visible_windows[i].end()){
        
      RECT win_rect; 
      GetWindowRect(window, &win_rect);

      // Position is middle of windows title bar
      std::vector<int> win_pos = {(win_rect.right + win_rect.left)/2, win_rect.top};

      for( auto &tile : grid){
        // Examine if windows is inside a tile
        if(tile[0] <= win_pos[0] && win_pos[0] <= tile[2] && tile[1] <= win_pos[1] && win_pos[1] <= tile[3]){
          // Place it to this tile
          MoveWindow(window, tile[0], tile[1], tile[2] - tile[0], tile[3] - tile[1], true);

          // Remove tiled window from window list
          visible_windows[i].erase(focused_window);

          // Remove tile from tile list
          auto this_tile_index = std::find(grid.begin(), grid.end(), tile);
          grid.erase(this_tile_index);

          // Exit loop
          break;
        }
      }
    }
    // ------------------------------------------------------

    // Place windows in grid --------------------------------
    for( auto &tile : grid){
      // Calculate tile center
      std::vector<int> tile_center = {(tile[2] + tile[0])/2, (tile[3] + tile[1])/2};

      // Find window with the least distance to tile center
      // Distance calculated from middle of windows title bar
      int min_distance = std::numeric_limits<int>::max();
      HWND window = NULL;
      for( auto &win : visible_windows[i]){
        RECT win_rect; 
        GetWindowRect(win, &win_rect);
        auto x_diff = tile_center[0] - (win_rect.right + win_rect.left)/2;
        auto y_diff = tile_center[1] - win_rect.top;
        auto distance = sqrt(pow(x_diff,2) + pow(y_diff,2));
        if(distance < min_distance){
          min_distance = distance;
          window = win;
        }
      }

      if (window != NULL) {
          // Move window to correct tile
          MoveWindow(window, tile[0], tile[1], tile[2] - tile[0], tile[3] - tile[1], true);
      }

      // Remove tiled window from window list
      auto window_index = std::find(visible_windows[i].begin(), visible_windows[i].end(), window);
      visible_windows[i].erase(window_index);

    }
    // ------------------------------------------------------
  }
}


int main(){
  // Define parameters
  int WINDOW_GAP = 10;
  int SCREEN_GAP = 15;

  MonitorRects screen_parameters;
  sort(screen_parameters.rcMonitors.begin(), screen_parameters.rcMonitors.end(), compareRect);

  // Instanciate g_pvdm
  CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&g_pvdm));

  // Arrange all visible windows to tiles
  while(true){
    tile_windows(screen_parameters.rcMonitors, WINDOW_GAP, SCREEN_GAP);
    Sleep(100);
  }

  CoUninitialize();

  return 0;
}
