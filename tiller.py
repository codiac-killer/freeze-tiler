import win32gui as w32
from math import log2, sqrt


def window_enumeration_handler(hwnd, window_list):
    # Get Window's rectangle
    window_rect = w32.GetWindowRect(hwnd)
    # Calculate window size
    win_size = (window_rect[2] - window_rect[0]) * (window_rect[3] - window_rect[1])

    # Filter system windows by name TODO: find out about those pesky apps | wont be tiled for now
    title = w32.GetWindowText(hwnd)
    title_flag = title != "" and title != "Program Manager" and title != "Calculator" and title != "Settings" and \
        title != "Microsoft Store" and title != "Movies & TV" and title != "Microsoft Text Input Application" and \
        title != "Task Manager"

    # Added window to list if conditions met
    if (not w32.IsIconic(hwnd)) and w32.IsWindowVisible(hwnd) and win_size and title_flag:
        window_list.append(hwnd)


def get_non_minimized_windows(monitor_parameters):
    # Get Non Minimized windows
    top_windows = []
    w32.EnumWindows(window_enumeration_handler, top_windows)

    # Determine regions of screens
    windows_p_screen = [[] for _ in monitor_parameters]

    # Divide window list by screen
    for win in top_windows:
        # Get Window's rectangle
        window_rect = w32.GetWindowRect(win)
        for i in range(len(monitor_parameters)):
            # IF window is lefter than the left screen
            if i == 0 and window_rect[0] < monitor_parameters[0][0]:
                windows_p_screen[i].append(win)
            # IF window starts somewhere in the screen
            elif (monitor_parameters[i][0] - 8) <= window_rect[0] < monitor_parameters[i][2]:
                windows_p_screen[i].append(win)
            # If window is righter than the right screen (probably wont happen)
            elif i == len(monitor_parameters) - 1 and window_rect[0] > monitor_parameters[i][2]:
                windows_p_screen[i].append(win)

    # return window list divided to screens
    return windows_p_screen


def tile_windows(monitor_parameters):
    # Find all visible windows
    visible_windows = get_non_minimized_windows(monitor_parameters)

    for windows, this_screen in zip(visible_windows, monitor_parameters):
        # Check if desktop is empty ----------------------------
        if not windows:
            continue
        # ------------------------------------------------------

        # Calculate Grid ---------------------------------------
        # Get screen dimensions
        screen_wid = this_screen[2] - this_screen[0]
        screen_height = this_screen[3] - this_screen[1]

        # Get working area
        grid_wid = screen_wid - 2*SCREEN_GAP
        grid_height = screen_height - 2*SCREEN_GAP
        # Initialize grid (1 tile)
        grid = [(this_screen[0] + SCREEN_GAP, this_screen[1] + SCREEN_GAP,
                 this_screen[0] + SCREEN_GAP + grid_wid, this_screen[1] + SCREEN_GAP + grid_height)]

        # Number of tiles
        num_of_tiles = len(windows)

        # Calculate tile boxes
        tile_index = 1  # which tile gets halved for next step
        power_of_tiles = 0  # log2(i)
        for tile in range(1, num_of_tiles):
            # slice tile in half if power_of_tiles even vertically else horizontally
            if power_of_tiles % 2 == 0:  # split vertically
                # pop old tile
                old_tile = grid.pop(-tile_index)

                # replace with two new tiles
                grid.insert(len(grid) - tile_index + 1,
                            (old_tile[0], old_tile[1], int((old_tile[0] + old_tile[2])/2 - WINDOW_GAP/2), old_tile[3]))
                grid.insert(len(grid) - tile_index + 1,
                            (int((old_tile[0] + old_tile[2])/2 + WINDOW_GAP/2), old_tile[1], old_tile[2], old_tile[3]))

            else:  # split horizontally
                # pop old tile
                old_tile = grid.pop(-tile_index)

                # replace with two new tiles
                grid.insert(len(grid) - tile_index + 1,
                            (old_tile[0], old_tile[1],
                             old_tile[2], int((old_tile[1] + old_tile[3]) / 2 - WINDOW_GAP / 2)))
                grid.insert(len(grid) - tile_index + 1,
                            (old_tile[0], int((old_tile[1] + old_tile[3]) / 2 + WINDOW_GAP / 2),
                             old_tile[2], old_tile[3]))

            # Check if log2(tile) is integer
            if log2(tile + 1).is_integer():
                power_of_tiles += 1
                # Reset tile_index
                tile_index = 1
            else:
                # Increase counter
                tile_index += 2
        # ------------------------------------------------------

        # Place focused window to tile if it is inside one -----
        # Get Rectangle of focused window
        window = w32.GetForegroundWindow()
        win_rect = w32.GetWindowRect(window)

        # Position is middle of windows title bar
        win_pos = (win_rect[2] + win_rect[0])/2, win_rect[1]

        for tile in grid:
            # Examine if windows is inside a tile
            if tile[0] <= win_pos[0] <= tile[2] and tile[1] <= win_pos[1] <= tile[3]:
                # Place it to this tile
                w32.MoveWindow(window, tile[0], tile[1],
                               tile[2] - tile[0], tile[3] - tile[1], True)

                # Remove tiled window from window list
                windows.remove(window)

                # Remove tile from tile list
                grid.remove(tile)

                # Exit loop
                break

        # ------------------------------------------------------
        print(grid)
        # Place windows in grid --------------------------------
        for tile in grid:
            # Calculate tile center
            tile_center = (tile[2] + tile[0])/2, (tile[3] + tile[1])/2

            list_of_distances = []
            # Find windows' distances to tile center
            # Distance calculated from middle of windows title bar
            for win in windows:
                win_rect = w32.GetWindowRect(win)
                x_diff = tile_center[0] - (win_rect[2] + win_rect[0])/2
                y_diff = tile_center[1] - win_rect[1]
                distance = sqrt(x_diff**2 + y_diff**2)
                list_of_distances.append([win, distance])

            # Find closest window to tile center
            window = sorted(list_of_distances, key=lambda a_entry: a_entry[1])[0]

            # Move window to correct tile
            w32.MoveWindow(window[0], tile[0], tile[1],
                           tile[2] - tile[0], tile[3] - tile[1], True)

            # Remove tiled window from window list
            windows.remove(window[0])

        # ------------------------------------------------------


if __name__ == "__main__":
    # Define parameters
    WINDOW_GAP = 10
    SCREEN_GAP = 15
    SCREEN_PARAMETERS = ((-1280, 54, 0, 1038), (0, 0, 1920, 1040))

    # Arrange all visible windows to tiles
    tile_windows(SCREEN_PARAMETERS)
