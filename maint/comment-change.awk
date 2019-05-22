function readfile(file,    tmp, save_rs)
{
    save_rs = RS
    RS = "^$"
    getline tmp < file
    close(file)
    RS = save_rs

    return tmp
}

BEGIN {
    found = 0;
    header = readfile(topdir "/maint/header")
}

!found && /\*\// {
    printf("   Copyright (C) 2019 Free Software Foundation, Inc.\n\n");

    printf ("%s", header)
    found = 1
}

!found && /Copyright \(c\)/,/\*\// {
    next
}

{ print }
