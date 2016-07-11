str=$(tail -n1 ../../tmp/ptpStats.log)
echo "$str" |cut -d ',' -f9
echo "$str" | cut -d ' ' -f1 | cut -d '-' -f1 #year
echo "$str" | cut -d ' ' -f1 | cut -d '-' -f2 #month
echo "$str" | cut -d ' ' -f1 | cut -d '-' -f3 #day
echo "$str" | cut -d ' ' -f2 | cut -d ',' -f1 | cut -d ':' -f1 #hour
echo "$str" | cut -d ' ' -f2 | cut -d ',' -f1 | cut -d ':' -f2 #min
echo "$str" | cut -d ' ' -f2 | cut -d ',' -f1 | cut -d ':' -f3 | cut -d '.' -f1 #second
echo "$str" | cut -d ' ' -f2 | cut -d ',' -f1 | cut -d ':' -f3 | cut -d '.' -f2 #millisecond