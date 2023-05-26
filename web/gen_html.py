#!/usr/bin/python3
from prettytable import PrettyTable
import re
 
# open csv file
a = open("a.csv", 'r')
 
# read the csv file
a = a.readlines()
 
# Separating the Headers
 
# headers for table
t = PrettyTable(["Date", "Host", "Version", "Result", "Note"])

# Adding the data
for i in range(0, len(a)):
    #print(a[i])
    d,h,v,s,n = a[i].split(',')
    s = re.sub('@', "\n", s) 
    t.add_row([d,h,v,s,n])

t.border = True 
#t.format = True
#t.add_row(a[i].split(','))
css = open("css.html", 'r')

code = t.get_html_string(sortby="Date", reversesort=True)
code = re.sub("fail", "<mark>fail</mark>", code)
html_file = open('index.html', 'w')
html_file.write(css.read())
html_file.write(code)

