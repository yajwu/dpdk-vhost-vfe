#!/usr/bin/python3
from prettytable import PrettyTable
import re
import markdown
import sys


mon=sys.argv[1]
print(mon)

# open csv file
a = open("a.csv", 'r')

# read the csv file
a = a.readlines()

# Separating the Headers

# headers for table
t = PrettyTable(["Date", "Host", "Version", "Result", "Note"])

# Adding the data
for i in range(0, len(a)):
    d,h,v,s,n = a[i].split(',')
    s = re.sub('@', "\n", s)
    t.add_row([d,h,v,s,n])

t.border = True
#t.format = True
#t.add_row(a[i].split(','))
# gen index page
css = open("css.html", 'r')

# gen index page
code = t[:10].get_html_string()
code = re.sub("fail", "<mark>fail</mark>", code)
code = re.sub("error", "<mark>error</mark>", code)
html_file = open('index.html', 'w')

with open('index.md', 'r') as f:
    text = f.read()
    html = markdown.markdown(text)
    html_file.write(html)

html_file.write(css.read())
html_file.write(code)

# gen month page
css = open("css.html", 'r')

code = t.get_html_string()
code = re.sub("fail", "<mark>fail</mark>", code)
code = re.sub("error", "<mark>error</mark>", code)
html_file = open(mon+".html", 'w')

with open('index.md', 'r') as f:
    text = f.read()
    html = markdown.markdown(text)
    html_file.write(html)

html_file.write(css.read())
html_file.write(code)


