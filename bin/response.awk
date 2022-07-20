BEGIN { format="%-15s %-3s\n"
        printf format, "IP", "STATUS CODE" }
/ [0-9][0-9][0-9] [A-Za-z]+ / { nr++; printf format, $5, $6 }

END { print "\n" "Total records: " nr }
