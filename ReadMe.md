- use arrow up, down, left, right to move the cursor
- Page Up to move up of the page, and vice versa for down
- Home key to move left of the page and End key to move right of the page
- Ctrl-q to quit 
- Ctrl-s to save changes
- If E.dirty is set, we will display a warning in the status bar, and require the user to press Ctrl-Q three more times in order to quit without saving.
- Pressing -> key and then Backspace is == Delete key. It deletes the char to the right of the cursor
Backspacing at the start of a line: When the user backspaces at the beginning of a line, append the contents of that line to the previous line, and then delete the current line. This effectively backspaces the implicit \n character in between the two lines to join them into one line.
- pressing the Delete key at the end of a line works as the user would expect, joining the current line with the next line
- The Enter key allows the user to insert new lines into the text, or split a line into two lines
- when the user runs ./hello with (no arguments), they willl get a blank file to edit. 
        
	- Press CTRL-S to save with the file name. Press Enter key to write it to disk then CTRL-Q to quit
        - else if the user press CTRL-S to save but without specified the name, ESC to cancel and then CTRL-Q 3 times to quit
        - the user can also press Backspace (or Ctrl-H, or Delete) in the input prompt 

- CTRL_F to use search feature. Will support (incremental search), meaning the file is searched after each keypress when the user is typing in their search query
- Search forward and backward: allow the user to advance to the next or previous match in the file using the arrow keys. The ↑ and ← keys will go to the previous match, and the ↓ and → keys will go to the next match.
- Detect Filetype: when a user open a C file in the editor, they should see numbers getting highlighted, and they should see c in the status bar where the filetype is displayed
        
	- When a user start up the editor with no arguments and save the file with a filename that ends in .c, they should see the filetype in the status bar change satisfyingly from no ft to c

- single line comments are cyan color
- string are magenta color
- numbers are red, search results(HL_MATCH) are blue, bur purple on vscode terminal
- common types name are green, actual keywords are yellow

- Non printable characters: fix the issue when runining hello and pass itself as the argument, hard to move the cursor arround. Every keypress causes the terminal to ding, because the audible bell character (7) is being printed out.Strings containing terminal escape sequences in the code are being printed out as actual escape sequences, because that’s how they’re stored in a raw executable.
        To prevent all that: translate nonprintable chars into printable ones:
        
	- Render the alphabetic control chars: (Ctrl-A = 1, Ctrl-B = 2, …, Ctrl-Z = 26) as the capital letters A through Z. The 0 byte like a control character. Ctrl-@ = 0, render it as an @ sign. Any other nonprintable characters we’ll render as a question mark (?). To differentiate these chars from their printable conterparts, use only black and white color
        
	- can test it out by pressing Ctrl-A, Ctrl-b and so on to insert those control characters into strings or comments, thye get the same color as the surrounding characters, just inverted

