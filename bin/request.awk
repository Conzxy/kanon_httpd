BEGIN{ url_width=40
       record_num=0
       format="%-15s %-" url_width "s %-7s %-9s %-10s\n"
       printf(format, "IP", "URL", "METHOD", "VERSION", "CONNECTION") }

/.+\[INFO\].+\/.* (GET|POST) HTTP\/[0-9]\.[0-9]+/ { 
  ip=$5
  url=$6
  method=$7
  ver=$8
  conn=$9
  record_num++
  if (length(url) > url_width) {
    printf(format, ip, substr(url, 1, url_width), method, ver, conn)
    substr_num=(length(url)/url_width)
    for (i = 1; i < substr_num; ++i)
      printf(format, "", substr(url, i*url_width+1, url_width), "", "", "")
  } else
    printf(format, ip, url, method, ver, conn) } 

END { print "\n" "Total records: " record_num }
