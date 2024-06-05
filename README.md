Part 1 –
Skeleton program was used as a baseline to build up our programs. For the first part, we
respectively implemented the background execution and the conversion of execvp into execv,
though we encountered many bugs in the latter. Again taking shell-skeleton.c as a base model,
we implemented cd and exit by changing a number of code blocks.


Part 2 –
In this part we implemented piping first trying to pipe 2 commands consecutively. However,
understanding the structure of shell-skeleton.c was truly challenging, and we ended up
producing many functions. Gradually we simplified these functions and improved their
functionality. In the end, we had our piping function, which is tested in other environments as
well, and works correctly. Desired outputs can be taken by testing it with different commands.


Part 3 –

1. Auto-complete
While implementing this functionality, we made use of helper functions such as
autocomplete_command which mainly handles the completion and listing of files depending
on the required case. The call to this autocomplete_command function is made inside the
process_command function and the buffer is modified inside the helper function, without
returning anything.

2. hdiff
This functionality is implemented again by using helper functions compareBinaryFiles and
compareTextFiles, both of which independently test for what their name suggests and return 0
or 1 while printing the desired information such as “2 different lines found”. This command is
implemented within process command just like “cd” command, and it works in the same way

3. Custom Command – mindmap
mindmap command takes no arguments and it creates a mindmap of the entered directory.
After typing the command “mindmap” and pressing enter, command line requests you to
provide a directory address and after entering that address (e.g.
/Users/akars20/comp304/starter-code/), it visually lists the subdirectories and files in a
beautiful format. This functionality is also implemented by using additional function
mindmap() and it is called inside process_command.


Part 4 –
This part is completed by coding 2 different files, that is, psvis.c and psvis_module.c, which
overwrites the provided file mymodule.c. Makefile is also created by us starting from scratch,
and it is properly tested in an outside environment. Only the visualization part could not be
implemented due to our limited technical knowledge, but the rest is working as specified.


References
https://www.informit.com/articles/article.aspx?p=368650
https://tldp.org/LDP/lkmpg/2.4/html/x354.htm
https://iq.opengenus.org/ls-command-in-c/
