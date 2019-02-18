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
    daterange_from = ""
    daterange_to = ""
    header = readfile(topdir "/maint/header")
}

!found && /Copyright \(c\) / {
    match($0, /([0-9]+), (([0-9]+), )?/, matches)
    daterange_from=matches[1]
    daterange_to=matches[3]
}

!found && /\*\// {
    if (daterange_to == "")
	printf("   Copyright (C) %s Free Software Foundation, Inc.\n\n",
		daterange_from)
    else
	printf("   Copyright (C) %s-%s Free Software Foundation, Inc.\n\n",
	       daterange_from, daterange_to)

    printf ("%s", header)
    found = 1
}

!found && /Copyright \(c\)/,/\*\// {
    next
}

{ print }
