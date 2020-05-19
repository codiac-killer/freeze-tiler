import win32gui as w32
from math import log2


def window_enumeration_handler(hwnd, window_list):
    # Get Window's rectangle
    window_rect = w32.GetWindowRect(hwnd)
    # Calculate window size
    win_size = (window_rect[2] - window_rect[0]) * (window_rect[3] - window_rect[1])

    # Filter system windows by name TODO: find out about those pesky apps | wont be tiled for now
    title = w32.GetWindowText(hwnd)
    title_flag = title != "" and title != "Program Manager" and title != "Calculator" and title != "Settings" and \
        title != "Microsoft Store" and title != "Movies & TV"

    # Added window to list if conditions met
    if (not w32.IsIconic(hwnd)) and w32.IsWindowVisible(hwnd) and win_size and title_flag:
        window_list.append(hwnd)


def get_non_minimized_windows():
    # Get Non Minimized windows
    top_windows = []
    w32.EnumWindows(window_enumeration_handler, top_windows)

    # Determine regions of screens
    # -8 to get the maximized windows (they protrude)
    # +1 at the end to get windows starting just at the edge
    regions = [-1288, -8, 1920 + 1]
    windows_p_screen = [[], []]

    # Divide window list by screen
    for win in top_windows:
        # Get Window's rectangle
        window_rect = w32.GetWindowRect(win)
        for i in range(len(regions)):
            # IF window starts somewhere in the screen
            if regions[i] <= window_rect[0] < regions[i + 1]:
                windows_p_screen[i].append(win)

    # return window list divided to screens
    return windows_p_screen


if __name__ == "__main__":
    # Define parameters
    WINDOW_GAP = 30
    SCREEN_GAP = 50
    SCREEN_PARAM = ((-1280, 54, 0, 1038), (0, 0, 1920, 1040))

    windows_of_screen = get_non_minimized_windows()

    for windows, screen_param in zip(windows_of_screen, SCREEN_PARAM):
        # Debugging --------------------------------------------
        # print(windows_of_screen.index(windows))
        # ------------------------------------------------------

        # Calculate Grid ---------------------------------------
        # Get screen dimensions
        screen_wid = screen_param[2] - screen_param[0]
        screen_height = screen_param[3] - screen_param[1]

        # Get working area
        grid_wid = screen_wid - 2*SCREEN_GAP
        grid_height = screen_height - 2*SCREEN_GAP
        # Initialize grid (1 tile)
        grid = [(screen_param[0] + SCREEN_GAP, screen_param[1] + SCREEN_GAP,
                 screen_param[0] + SCREEN_GAP + grid_wid, screen_param[1] + SCREEN_GAP + grid_height)]

        # Number of tiles
        num_of_tiles = len(windows)

        # Calculate tile boxes
        tile_index = 1  # which tile gets halved for next step
        power_of_tiles = 0  # log2(i)
        for tile in range(num_of_tiles-1):
            # slice tile in half if power_of_tiles even vertically else horizontally
            if power_of_tiles % 2 == 0:  # split vertically
                # pop old tile
                old_tile = grid.pop(-tile_index)

                # replace with two new tiles
                grid.insert(len(grid) - tile_index + 1,
                            (old_tile[0], old_tile[1], old_tile[2]/2 - WINDOW_GAP/2, old_tile[3]))
                grid.insert(len(grid) - tile_index + 1,
                            (old_tile[2]/2 + WINDOW_GAP/2, old_tile[1], old_tile[2], old_tile[3]))

            else:  # split horizontally
                # pop old tile
                old_tile = grid.pop(-tile_index)

                # replace with two new tiles
                grid.insert(len(grid) - tile_index + 1,
                            (old_tile[0], old_tile[1], old_tile[2], old_tile[3] / 2 - WINDOW_GAP / 2))
                grid.insert(len(grid) - tile_index + 1,
                            (old_tile[0], old_tile[3] / 2 + WINDOW_GAP / 2, old_tile[2], old_tile[3]))

            # Check if log2(tile) is integer
            if log2(tile + 1).is_integer():
                power_of_tiles += 1
                # Reset tile_index
                tile_index = 1
            else:
                # Increase counter
                tile_index += 2
        # ------------------------------------------------------

        # Place windows in grid
        for window in windows:
            # print(w32.GetWindowText(window))
            new_rect = list(map(int, grid.pop(0)))

            print(new_rect)

            w32.MoveWindow(window, new_rect[0], new_rect[1],
                           new_rect[2] - new_rect[0], new_rect[3] - new_rect[1], True)
