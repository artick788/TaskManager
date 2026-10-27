# TaskManager
A small planner app build around a TODO app

## Features
- Home screen with all open tasks grouped by due date (overdue, today, tomorrow, this week, later) and sorted by priority
- Organise tasks in nested folders and lists, e.g. a folder `AURA` with the lists `UI tasks` and `Backend tasks`
- Two kinds of lists:
  - **Task lists**: tasks with a priority and a due date, shown on the home screen
  - **Checklists**: plain items to tick off, e.g. a packing list in a `Packlists` folder
- Reset every task of a list or of a whole folder back to TODO, with undo
- Drag and drop to reorder tasks, move tasks to another list and move lists and folders around
- Optionally starts when you log in (Settings > Start when I log in)
- Changes are saved automatically to `~/.config/TaskManager/settings_TaskManager.json`

## Usage
1. Clone the repository:
   ```bash
   git clone git@github.com:artick788/TaskManager.git --recursive
   ```
   Don't forget to use the `--recursive` flag to clone the submodules as well.
2. Navigate to the project directory:
   ```bash
   cd TaskManager
   ```
3. Run install script
   This will build the app from source and install it on your system.
   ```bash
   chmod +x install.sh
   ./install.sh
   ```
4. Launch the app from the terminal:
   ```bash
   TaskManager
   ```
   If the app does not launch, you might need to add it to your PATH:
   ```bash
   export PATH="$HOME/.local/bin:$PATH"
   ```
   Consider adding this to your bashrc or zshrc file to make it permanent.
5. Enable Settings > Start when I log in from the installed app, so the autostart entry points at the installed executable.

## Shortcuts
- `Ctrl+N`: add a task to the open list
- `Ctrl+S`: save now
- `Delete`: delete the selected task
- `Escape`: close the task details
