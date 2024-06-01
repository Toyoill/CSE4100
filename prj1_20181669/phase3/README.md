README

To execute this program, enter the following command to compile the program.

####
make
####

The myshell program will be created.

This program supports the following commands:
###
cd
ls
mkdir
rmdir
touch
cat
echo
history
sort
less
fg
bg
jobs
kill
exit
###
The output of above commands is similar to the result of a Linux command.
Please refer to Linux docs for more information.

You can use pipeline command. Please refer to the following commands as a reference.
###
ls | grep filename
cat $filename | less
cat $filename | grep -i "abc" | sort -r 
###

You can execute a command in background. If you want to execute command in background,
please add & character after commandline.

If you want to suspend foreground process, please enter ctrl+Z.

If you enter ctrl+c, currently running program in myshell will be terminate.
