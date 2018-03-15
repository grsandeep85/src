#!/usr/bin/env python3
# ex:ts=4:sw=4:sts=4:et
# -*- tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*-
#
# Copyright (c) 2017 Wind River Systems, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#
# <credits>
#  { David Reyna,  david.reyna@windriver.com,  },
# </credits>
#

import sys
import sqlite3
import re
from operator import itemgetter, attrgetter, methodcaller

#
# Note: you can find these tables and their members directly
# from the Toaster database, for example:
#   $ sqlite3 toaster.sqlite
#   sqlite> .tables
#   sqlite> .schema orm_build
# Note: you can also use SQL tools to examine the schema
#   $ sqlitebrowser toaster.sqlite
#

# Index into orm_build table
BUILD_ORM_ID=0
BUILD_ORM_MACHINE=1
BUILD_ORM_STARTED_ON=4
BUILD_ORM_COMPLETED_ON=5
BUILD_ORM_OUTCOME=6
BUILD_ORM_PROJECT_ID=9
# Index into orm_target table
TARGET_ORM_TARGET=1
TARGET_ORM_TASK=2
TARGET_ORM_BUILD_ID=6
# Index into orm_project table
PROJECT_ORM_NAME=1
# Index into orm_task table
TASK_ORM_NAME=6
TASK_ORM_START=22
TASK_ORM_STOP=21
TASK_ORM_BUILD_ID=15
TASK_ORM_RECIPE_ID=16
# Index into orm_recipe table
RECIPE_ORM_NAME=2
# Index into taskList
TASK_RECIPE=0
TASK_NAME=1
TASK_START=2
TASK_STOP=3
TASK_OVERCOUNT=4
TASK_OVERLIST=5
# Index into recipeList
RECIPE_NAME=0
RECIPE_START=1
RECIPE_STOP=2
RECIPE_OVERCOUNT=3
RECIPE_OVERLIST=4
# Index into taskTimeList,recipeTimeList
START=1
STOP=2
TIME_STATE=0
TIME_OVERCOUNT=1
TIME_TIME=2
TIME_RECIPE=3
TIME_TASK=4

# The database objects
database_file = None
conn = None
build_cursor = None
project_cursor = None
target_cursor = None

# The data structures
build=None
taskList = []
recipeList = []
taskTimeList = []
recipeTimeList = []
build_data={}

# Statistics for displaying columns
recipe_length_max = 0
task_length_max = 0
task_execute_max = 0
recipe_execute_max = 0

# debug support
RECORD_MAX=None  # None for all, else max number of records to read

COMMAND_LINE_PROJ = "Command Line"
NO_TARGET = "No_Target"
NO_TASK = "No_Task"

#################################
# show help
#

def show_help():
    print('=== event_overlap.py ===')
    print('Commands:')
    print(' ?                           : show help')
    print(' b,build   [build_id]        : show or select builds')
    print(' d,data                      : show histogram data')
    print(' t,task    [task]            : show task database')
    print(' r,recipe  [recipe]          : show recipes database')
    print(' e,events  [task]            : show task time events')
    print(' E,Events  [recipe]          : show recipe time events')
    print(' o,overlap [task|0|n]        : show task|zero|n_max execution overlaps')
    print(' O,Overlap [recipe|0|n]      : show recipe|zero|n_max execution overlaps')
    print(' g,graph   [task]   [> file] : graph task execution overlap')
    print(' G,Graph   [recipe] [> file] : graph recipe execution overlap')
    print(' h,html    [task]   [> file] : HTML graph task execution overlap [to file]')
    print(' H,Html    [recipe] [> file] : HTML graph recipe execution overlap [to file]')
    print(' q,quit                      : quit')
    print('')
    print("Examples: ")
    print("  * Recipe/task filters accept wild cards, like 'native-*, '*-lib*'")
    print("  * Recipe/task filters get an automatic wild card at the end")
    print("  * Task names are in the form 'recipe:task', so 'acl*patch' ")
    print("    will specifically match the 'acl*:do_patch' task")
    print("  * Use 'o 2' for the tasks in the two highest overlap count sets")
    print("  * Use 'O 0' for the recipes with zero overlaps")
    print("  * Use 'd' to see the distribution of parallel and overlap execution")
    print('')

#################################
# set up task/recipe filter
#   auto-append wildcard
#   convert '*' to regex
#   return compiled regex filter

def prepare_filter(filter_string):
    # auto wildcard at end
    if (0 == len(filter_string)) or ('*' != filter_string[-1]):
        filter_string = filter_string + '*'
    filter_string = filter_string.replace('*','.*')
    return re.compile(filter_string)

#################################
# print to STDOUT or file

output_file=''
output_fd=None

def output_file_action(action,file):
    global output_file,output_fd
    if '' != file:
        if "open" == action:
            output_file=file
            try:
                output_fd=open(output_file, 'w')
            except:
                print("\nERROR: Could not open file '%s'\n" % output_file)
                return False
        if "close" == action:
            output_fd.close()
            print("\nDone: file '%s' created" % output_file)
            output_fd=None
    else:
        output_file=''
        output_fd=None
    return True

def event_print(line,end=' '):
    if '' == output_file:
        if '' == end:
            print(line,end='')
        else:
            print(line)
    else:
        if '' == end:
            print(line,end='',file=output_fd)
        else:
            print(line,file=output_fd)
            
#################################
# connect to database
#

def connect_database(filename):
    global database_file,conn
    global build_cursor,project_cursor,target_cursor

    database_file = filename
    conn = sqlite3.connect(database_file)
    if None == conn:
        print("ERROR: %s is not an sqlite database" % database_file)
        sys.exit(1)
    build_cursor = conn.cursor()
    project_cursor = conn.cursor()
    target_cursor = conn.cursor()

#################################
# show build list from database
#

def build_outcome(x):
    return {
        '0': 'SUCCEEDED',
        '1': 'FAILED',
        '2': 'IN_PROGRESS',
        '3': 'CANCELLED',
    }[x]

def fetch_build_metadata(build):
    global build_data
    build_data={}
    # read this build's meta information
    build_data['id']=build[BUILD_ORM_ID]
    build_data['machine']=build[BUILD_ORM_MACHINE]
    build_data['started_on']=build[BUILD_ORM_STARTED_ON]
    build_data['completed_on']=build[BUILD_ORM_COMPLETED_ON]
    build_data['outcome']=build[BUILD_ORM_OUTCOME]
    project_id=build[BUILD_ORM_PROJECT_ID]
    # look up this build's project name
    project_cursor.execute("SELECT * FROM orm_project where id = '%s'" % project_id)
    project = project_cursor.fetchone()
    if None == project:
        build_data['project'] = COMMAND_LINE_PROJ
    else:
        build_data['project']=project[PROJECT_ORM_NAME]
    # look up this build's target information
    target_cursor.execute("SELECT * FROM orm_target where build_id = '%s'" % build[BUILD_ORM_ID])
    target = target_cursor.fetchone()
    if None == target:
        build_data['target']=NO_TARGET
        build_data['task']=NO_TASK
    else:
        build_data['target']=target[TARGET_ORM_TARGET]
        build_data['task']=target[TARGET_ORM_TASK]

def show_builds():
    print("List of available builds:")
    build_cursor.execute("SELECT * FROM orm_build")
    build = build_cursor.fetchone()
    while build != None:
        fetch_build_metadata(build)
        print("  BuildId=%s) CompletedOn=%s, Outcome=%s, Project=%s, Target=%s, Task=%s" %
            (build_data['id'],build_data['completed_on'],build_outcome(str(build_data['outcome'])),
            build_data['project'],build_data['target'],build_data['task']) )
        build = build_cursor.fetchone()
    return ""

#################################
# Fetch build data from database
#

def fetch_build_data(build_id):
    global build
    global taskList, recipeList, taskTimeList, recipeTimeList
    global recipe_length_max, task_length_max, task_execute_max, recipe_execute_max

    taskList = []
    recipeList = []
    taskTimeList = []
    recipeTimeList = []
    recipe_length_max = 0
    task_length_max = 0
    max_records = RECORD_MAX

    print("Fetching build #%d" % build_id)

    build_cursor.execute("SELECT * FROM orm_build where id = '%s'" % build_id)
    build=build_cursor.fetchone()
    fetch_build_metadata(build)

    c = conn.cursor()
    d = conn.cursor()

    # Fetch the build's tasks from the database
    c.execute("SELECT * FROM orm_task where build_id = '%s'" % build_id)
    task = c.fetchone()
    if None == task:
        build=None
        print("ERROR: No build or tasks found for this build id!")
        return False

    while None != task:
        # fetch the task's parent recipe name
        d.execute("SELECT * FROM orm_recipe where id = '%s'" % task[TASK_ORM_RECIPE_ID])
        recipe = d.fetchone()

        # get maximum string lengths
        if recipe_length_max < len(recipe[RECIPE_ORM_NAME]):
            recipe_length_max = len(recipe[RECIPE_ORM_NAME])
        if task_length_max < len(task[TASK_ORM_NAME]):
            task_length_max = len(task[TASK_ORM_NAME])

        # Fix time data for cached builds (time == None)
        task_start = task[TASK_ORM_START];
        if task_start == None:
            task_start = 'None'
        task_stop = task[TASK_ORM_STOP];
        if task_stop == None:
            task_stop = 'None'

        # Add the taskList entry
        taskList.append( [recipe[RECIPE_ORM_NAME],task[TASK_ORM_NAME],task_start, task_stop, 0, [] ])

        # Append the Task time start and stop entires
        taskTimeList.append( [START,0, task_start,recipe[RECIPE_ORM_NAME],task[TASK_ORM_NAME]] )
        taskTimeList.append( [STOP ,0, task_stop ,recipe[RECIPE_ORM_NAME],task[TASK_ORM_NAME]] )

        # Set the recipe time span
        for r in recipeList:
            # set the recipe's stop time from its last task
            if recipe[RECIPE_ORM_NAME] == r[RECIPE_NAME]:
                if r[RECIPE_STOP] < task_stop:
                    r[RECIPE_STOP] = task_stop
                break
        else:
            # first task for this recipe, set the recipe start time
            recipeList.append( [recipe[RECIPE_ORM_NAME], task_start, task_stop, 0, [] ])

        # Loop
        if None == max_records:
            task = c.fetchone()
        elif max_records > 0:
            max_records -= 1
            task = c.fetchone()
        else:
            task = None

    # Compute the overlapping tasks
    for t in taskList:
        for tt in taskList:
            if t != tt:
                if (t[TASK_START] < tt[TASK_STOP]) and (t[TASK_STOP] > tt[TASK_START]):
                    t[TASK_OVERLIST].append(tt[TASK_RECIPE]+':'+tt[TASK_NAME])
                    t[TASK_OVERCOUNT] += 1

    # Compute the overlapping recipes (over the span of the recipe's tasks)
    for r in recipeList:
        # append the Recipe time start and stop entires
        recipeTimeList.append( [START,0,r[RECIPE_START],r[RECIPE_NAME],''] )
        recipeTimeList.append( [STOP ,0,r[RECIPE_STOP] ,r[RECIPE_NAME],''] )
        for rr in recipeList:
            if r != rr:
                if (r[RECIPE_START] < rr[RECIPE_STOP]) and (r[RECIPE_STOP] > rr[RECIPE_START]):
                    r[RECIPE_OVERLIST].append(rr[RECIPE_NAME])
                    r[RECIPE_OVERCOUNT] += 1

    # sort the lists
    taskList.sort(key=lambda item: item[TASK_START])
    taskList.sort(key=lambda item: item[TASK_RECIPE])
    recipeList.sort(key=lambda item: item[RECIPE_NAME])
    taskTimeList.sort(key=lambda item: item[TIME_TIME])
    recipeTimeList.sort(key=lambda item: item[TIME_TIME])

    # count the task's max thread parallelism
    count=0
    task_execute_max=0
    for t in taskTimeList:
        if START == t[TIME_STATE]:
            count += 1;
            if task_execute_max < count:
                task_execute_max = count
        else:
            count -= 1;
        t[TIME_OVERCOUNT] = count;

    # count the recipe's max thread parallelism
    count=0
    recipe_execute_max=0
    for t in recipeTimeList:
        if START == t[TIME_STATE]:
            count += 1;
            if recipe_execute_max < count:
                recipe_execute_max = count
        else:
            count -= 1;
        t[TIME_OVERCOUNT] = count;

    print("Build: CompletedOn=%s, Outcome=%s, Project='%s'" %
            (build_data['completed_on'],build_outcome(str(build_data['outcome'])),build_data['project']) )
    print("       Target='%s', Task='%s', Machine='%s'" %
            (build_data['target'],build_data['task'],build_data['machine']) )
    print('Success: build #%d, Task Count=%d, Recipe Count=%d' % (build_id, len(taskList),len(recipeList)) )
    return True

#####################################
# compute and display histogram data

def compute_histogram (list, key, isFilter, description, is_html=False):
    HIST_MAX=2000
    hist=[]
    hist_top=0
    for i in range(HIST_MAX):
        hist.append(0)
    for t in list:
        if not isFilter or (START == t[TIME_STATE]):
            hist[t[key]] += 1;
            if t[key] > hist_top:
                hist_top = t[key]
    if not is_html:
        print("Histogram:"+description)
        print("    ", end='')
        for i in range(0,10):
            print(" {:3}".format(i), end='')
        print('')
        print("    ", end='')
        for i in range(0,10):
            print("----", end='')
        for i in range(hist_top+1):
            if 0 == (i % 10):
                print("")
                print("{:3})".format(i), end='')
            print(" {:3}".format(hist[i]), end='')
        print("")   # finish the last line
        print("")
    else:
        event_print('<p>Histogram:%s</p>' % description)
        event_print('<table border="1">')
        event_print('  <thead>')
        event_print('    <th></th>', end='')
        for i in range(0,10):
            event_print("<th>{:3}</th>".format(i), end='')
        event_print('')
        event_print('  </thead>')
        event_print('  <tbody>')
        event_print('    <tr>')
        for i in range(hist_top+1):
            if 0 == (i % 10):
                if 0 != i:
                    event_print('</tr>')
                event_print("    <tr><td>{:3})</td>".format(i), end='')
            event_print("<td>{:3}</td>".format(hist[i]), end='')
        event_print("</tr>")
        event_print('  </tbody>')
        event_print('</table>')
        event_print('<BR><BR>')

def display_statistics(is_html=False):
    compute_histogram(taskTimeList, TIME_OVERCOUNT, True,
        "For each task, max number of tasks executing in parallel",is_html)
    compute_histogram(recipeTimeList, TIME_OVERCOUNT, True,
        "For each recipe's task set, max number of recipes executing in parallel",is_html)
    compute_histogram(taskList, TASK_OVERCOUNT, False,
        "For each task, max number of tasks that overlap its build",is_html)
    compute_histogram(recipeList, RECIPE_OVERCOUNT, False,
        "For each recipe's task set, max number of recipes that overlap its build",is_html)

#################################
# display task and recipe tables
#

def display_tasks(filter_string,show_overlaps):
    # auto wildcard at end
    if (0 == len(filter_string)) or ('*' != filter_string[-1]):
        filter_string = filter_string + '*'
    filter_string = filter_string.replace('*','.*')
    prog = re.compile(filter_string)
    if show_overlaps:
        print('Task Table (Recipe,Task,Start,Stop,Overlap count,Overlap list):')
        for t in taskList:
            if prog.match(t[TASK_RECIPE]+':'+t[TASK_NAME]):
                print('  '+str(t))
    else:
        print('Task Table (Recipe,Task,Start,Stop,Overlap count):')
        for t in taskList:
            if prog.match(t[TASK_RECIPE]+':'+t[TASK_NAME]):
                print('  %s,%s,%s.%d' % (t[TASK_RECIPE]+':'+t[TASK_NAME],t[TASK_START],t[TASK_STOP],t[TASK_OVERCOUNT]))

def display_recipes(filter_string,show_overlaps):
    # auto wildcard at end
    if (0 == len(filter_string)) or ('*' != filter_string[-1]):
        filter_string = filter_string + '*'
    filter_string = filter_string.replace('*','.*')
    prog = re.compile(filter_string)
    if show_overlaps:
        print("Recipe Table (Recipe,Start,Stop,Overlap count,Overlap list):")
        for r in recipeList:
            if prog.match(r[RECIPE_NAME]):
                print("  "+str(r))
    else:
        print('Task Table (Recipe,Task,Start,Stop,Overlap count):')
        for r in recipeList:
            if prog.match(r[RECIPE_NAME]):
                print('  %s,%s,%s.%d' % (r[RECIPE_NAME],r[RECIPE_START],r[RECIPE_STOP],r[RECIPE_OVERCOUNT]))

#################################
# display time event lists
#

def display_task_events(filter_string):
    prog = prepare_filter(filter_string)
    print('Task Event List (State,Overlap Count,Time,Recipe,Task):')
    for t in taskTimeList:
        if START == t[TIME_STATE]:
            if prog.match(t[TIME_RECIPE]+':'+t[TIME_TASK]):
                print('  '+str(t))

def display_recipe_events(filter_string):
    prog = prepare_filter(filter_string)
    print('Recipe Event List (State,Overlap Count,Time,Recipe):')
    for r in recipeTimeList:
        if START == r[TIME_STATE]:
            if prog.match(r[TIME_RECIPE]):
                print('  '+str(r))

#################################
# display overlap lists
#
# if empty filter_string, print all entries
# if filter_string, print all matching entries
# if filter_string=0, print all entries with zero overlaps
# if filter_string=n, print the n top maximum overlap sets
#

def display_task_overlaps(filter_string,file):
    if not output_file_action('open',file):
        return
    if '0' == filter_string:
        event_print('\nTasks with zero overlap:')
        for t in taskList:
            if 0 == t[TASK_OVERCOUNT]:
                event_print('  '+t[TASK_RECIPE]+':'+t[TASK_NAME])
    elif filter_string.isdigit():
        max_count = int(filter_string)
        event_print('\nTasks with maximum overlap:')
        maxTasks = sorted(taskList, key=itemgetter(TASK_OVERCOUNT), reverse=True)
        last_max=maxTasks[0][TASK_OVERCOUNT]
        for t in maxTasks:
            if last_max != t[TASK_OVERCOUNT]:
                last_max = t[TASK_OVERCOUNT]
                max_count -= 1
                if 0 == max_count:
                    break
            event_print(t[TASK_RECIPE]+':'+t[TASK_NAME]+', COUNT=' + str(t[TASK_OVERCOUNT]) + ')')
            for o in t[TASK_OVERLIST]:
                event_print("  %s" % o)
    else:
        prog = prepare_filter(filter_string)
        event_print('\nTask overlap list (by recipe):')
        for t in taskList:
            if prog.match(t[TASK_RECIPE]+':'+t[TASK_NAME]):
                event_print(t[TASK_RECIPE]+':'+t[TASK_NAME]+', COUNT=' + str(t[TASK_OVERCOUNT]) + ')')
                if 0 == t[TASK_OVERCOUNT]:
                    event_print('  Zero overlaps')
                else:
                    for o in t[TASK_OVERLIST]:
                        event_print('  %s' % o)
        if 0 == len(taskList):
            event_print("  There are no tasks with zero overlap")
    event_print('')
    output_file_action('close',file)

def display_recipe_overlaps(filter_string,file):
    if not output_file_action('open',file):
        return
    if '0' == filter_string:
        event_print("\nRecipes with zero overlap:")
        for r in recipeList:
            if 0 == r[RECIPE_OVERCOUNT]:
                event_print('  '+r[RECIPE_NAME])
    elif filter_string.isdigit():
        max_count = int(filter_string)
        event_print("\nRecipes with maximum overlap:")
        maxRecipes = sorted(recipeList, key=itemgetter(RECIPE_OVERCOUNT), reverse=True)
        last_max=maxRecipes[0][RECIPE_OVERCOUNT]
        for r in maxRecipes:
            if last_max != r[RECIPE_OVERCOUNT]:
                last_max = r[RECIPE_OVERCOUNT]
                max_count -= 1
                if 0 == max_count:
                    break
            event_print(r[RECIPE_NAME]+', COUNT=' + str(r[RECIPE_OVERCOUNT]) + ')')
            for o in r[RECIPE_OVERLIST]:
                event_print("  %s" % o)
    else:
        prog = prepare_filter(filter_string)
        event_print("\nRecipe overlap list:")
        for r in recipeList:
            if prog.match(r[RECIPE_NAME]):
                event_print(r[RECIPE_NAME]+', COUNT=' + str(r[RECIPE_OVERCOUNT]) + ')')
                if 0 == r[RECIPE_OVERCOUNT]:
                    event_print("  Zero overlaps")
                else:
                    for o in r[RECIPE_OVERLIST]:
                        event_print("  %s" % o)
        if 0 == len(recipeList):
            event_print("  There are no recipes with zero overlap")
    event_print('')
    output_file_action('close',file)

#################################
# graph the overlap data
#  Text and HTML output
#

threads=[]
thread_filter=[]
thread_class=[]

def display_html_prolog(columns,isTask):
    event_print('<?xml version="1.0" encoding="UTF-8"?>')
    event_print('<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">')
    event_print('  <html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">')
    event_print('<head>')
    event_print('  <style>')
    event_print('    table, th, td {text-align:center;min-width:50px}')
    event_print('    td.recipe {text-align:left}')
    event_print('    td.recipe_filter {text-align:left;color:#FF4040}')
    # define the background color classes for First/continue event, Odd/Even Column, A/B event in column
    event_print('    td.oaf {background-color:#4040FF}')   # First   ,Odd,A: blue
    event_print('    td.oac {background-color:#c0c0FF}')   # Continue,Odd,A: light blue
    event_print('    td.oax {}')
    event_print('    td.obf {background-color:#40f040}')   # First   ,Odd,B: green
    event_print('    td.obc {background-color:#c0f0c0}')   # Continue,Odd,B: light green
    event_print('    td.obx {}')
    event_print('    td.eaf {background-color:#FF4040}')   # First   ,Even,A: red
    event_print('    td.eac {background-color:#FFc0c0}')   # Continue,Even,A: light red
    event_print('    td.eax {}')
    event_print('    td.ebf {background-color:#FF8020}')   # First   ,Even,B: orange
    event_print('    td.ebc {background-color:#FFcc40}')   # Continue,Even,B: light orange
    event_print('    td.ebx {}')
    event_print('  </style>')
    event_print('</head>')
    event_print('<body>')
    event_print(' <BR>')
    if isTask:
        event_print('<p><BIG><B>Task Execution Overlap Table</B></BIG></p>')
        event_print('<p>(derived from the Toaster event database)</p>')
    else:
        event_print('<p><BIG><B>Recipe Execution Overlap Table</B></BIG></p>')
        event_print('<p>(derived from the Toaster event database)</p>')
    event_print(' <BR>')

    event_print("<p>CompletedOn=%s, Outcome=%s, Project='%s', Target='%s', Task='%s', Machine='%s'</p>" %
            (build_data['completed_on'],build_outcome(str(build_data['outcome'])),
            build_data['project'],build_data['target'],build_data['task'],build_data['machine']) )
    event_print('<p>Task Count=%d, Recipe Count=%d</p>' %
            (len(taskList),len(recipeList)) )

    event_print(' <BR>')
    event_print('<table border="1">')
    event_print('  <thead>')
    for c in range(columns):
        event_print('    <th>%d</th>' % (c+1))
    if isTask:
        event_print('    <th>Recipe:Task</th>')
    else:
        event_print('    <th>Recipe</th>')
    event_print('  </thead>')
    event_print('  <tbody>')

def display_html_line(columns,task,action,position,is_filtered_task):
    if '-' != action:
        event_print('    <tr>',end='')
    for i in range(columns):
        if i == position:
            if is_filtered_task:
                content='['+action+']'
            else:
                content=action
            if '+' == action:
                # transition to first
                if 'oax' == thread_class[i]:
                    thread_class[i] = 'oaf'
                elif 'obx' == thread_class[i]:
                    thread_class[i] = 'obf'
                elif 'eax' == thread_class[i]:
                    thread_class[i] = 'eaf'
                elif 'ebx' == thread_class[i]:
                    thread_class[i] = 'ebf'
            else:
                # transition to toggled none
                if thread_class[i] in ('oaf','oac'):
                    thread_class[i] = 'obx'
                elif thread_class[i] in ('obf','obc'):
                    thread_class[i] = 'oax'
                elif thread_class[i] in ('eaf','eac'):
                    thread_class[i] = 'ebx'
                else:
                    thread_class[i] = 'eax'
        elif '' != threads[i]:
            content=thread_filter[i]
            # if first transition to continued
            if 'oaf' == thread_class[i]:
                thread_class[i] = 'oac'
            elif 'obf' == thread_class[i]:
                thread_class[i] = 'obc'
            elif 'eaf' == thread_class[i]:
                thread_class[i] = 'eac'
            elif 'ebf' == thread_class[i]:
                thread_class[i] = 'ebc'
        else:
            content=''
        if '-' != action:
            event_print('<td class="%s">%s</td>' % (thread_class[i],content), end='')
    if '-' != action:
        if is_filtered_task:
            event_print('<td class="recipe_filter">'+task+'</td></tr>')
        else:
            event_print('<td class="recipe">'+task+'</td></tr>')

def display_html_epilog():
    event_print('  </tbody>')
    event_print('</table>')
    event_print('<BR><BR>')
    display_statistics(True)
    event_print('<body>')

def display_thread_line(columns,task,action,position,is_filtered_task):
    event_print(' |',end='')
    for i in range(columns):
        if i == position:
            event_print(' '+action+' |',end='')
        elif '' != threads[i]:
            event_print(' %s |' % thread_filter[i],end='')
        else:
            event_print('   |',end='')
    if is_filtered_task:
        event_print(' *'+task)
    else:
        event_print(' '+task)

def graph_task_overlaps(is_html,filter_string,file):
    global threads,thread_filter
    if not output_file_action('open',file):
        return
    if is_html:
        display_html_prolog(task_execute_max,True)
    else:
        event_print('\nGraph Task Overlaps:')
    threads=[]
    thread_filter=[]
    for i in range(task_execute_max):
        threads.append('')
        thread_filter.append('')
        if 1 == (i % 2):
            thread_class.append('eax')
        else:
            thread_class.append('oax')
    # setup the filter, if any
    match_tasks=[]
    match_overlaps=[]
    if filter_string:
        prog = prepare_filter(filter_string)
        for t in taskList:
            if prog.match(t[TASK_RECIPE]+":"+t[TASK_NAME]):
                match_tasks.append(t[TASK_RECIPE]+":"+t[TASK_NAME])
                match_overlaps.append(t[TASK_RECIPE]+":"+t[TASK_NAME])
                match_overlaps.extend(t[TASK_OVERLIST])
    for t in taskTimeList:
        task=t[TIME_RECIPE]+":"+t[TIME_TASK]
        position=0
        action=' '
        try:
            is_filtered_task = (0 <= match_tasks.index(task))
        except:
            is_filtered_task=False
        if filter_string and not task in match_overlaps:
            continue
        if START == t[TIME_STATE]:
            for i in range(task_execute_max):
                if '' == threads[i]:
                    threads[i]=task
                    if is_filtered_task:
                        thread_filter[i]='*'
                    else:
                        thread_filter[i]='"'
                    position=i
                    action='+'
                    break
        else:
            for i in range(task_execute_max):
                if task == threads[i]:
                    threads[i]=''
                    position=i
                    action='-'
                    break
        if is_html:
            display_html_line(task_execute_max,task,action,position,is_filtered_task)
        else:
            display_thread_line(task_execute_max,task,action,position,is_filtered_task)
    if is_html:
        display_html_epilog()
    output_file_action('close',file)
    event_print('')

def graph_recipe_overlaps(is_html,filter_string,file):
    global threads,thread_filter
    if not output_file_action('open',file):
        return
    if is_html:
        display_html_prolog(recipe_execute_max,False)
    else:
        event_print("\nGraph Recipe Overlaps:")
    threads=[]
    for i in range(recipe_execute_max):
        threads.append('')
        thread_filter.append('')
        if 1 == (i % 2):
            thread_class.append('eax')
        else:
            thread_class.append('oax')
    # setup the filter, if any
    match_recipes=[]
    match_overlaps=[]
    if filter_string:
        prog = prepare_filter(filter_string)
        for r in recipeList:
            if prog.match(r[RECIPE_NAME]):
                match_recipes.append(r[RECIPE_NAME])
                match_overlaps.append(r[RECIPE_NAME])
                match_overlaps.extend(r[RECIPE_OVERLIST])
    for t in recipeTimeList:
        recipe=t[TIME_RECIPE]
        position=0
        action=' '
        try:
            is_filtered_task = (0 <= match_recipes.index(recipe))
        except:
            is_filtered_task=False
        if filter_string and not recipe in match_overlaps:
            continue
        if START == t[TIME_STATE]:
            for i in range(recipe_execute_max):
                if '' == threads[i]:
                    threads[i]=recipe
                    if is_filtered_task:
                        thread_filter[i]='*'
                    else:
                        thread_filter[i]='"'
                    position=i
                    action='+'
                    break
        else:
            for i in range(recipe_execute_max):
                if recipe == threads[i]:
                    threads[i]=''
                    thread_filter[i]=''
                    position=i
                    action='-'
                    break
        if is_html:
            display_html_line(recipe_execute_max,recipe,action,position,is_filtered_task)
        else:
            display_thread_line(recipe_execute_max,recipe,action,position,is_filtered_task)

    if is_html:
        display_html_epilog()
    output_file_action('close',file)
    event_print('')

#################################
# main loop
#

def main(argv):
    global build

    print("\nWelcome to event_overlap.py: enter '?' for help\n")

    # connect to the database
    if 0 < len(argv):
        filename=argv[0]
    else:
        filename='toaster.sqlite'
    connect_database(filename)

    # fetch the default build data
    try:
        build_cursor.execute('SELECT * FROM orm_build')
        build=build_cursor.fetchone()
    except:
        print("ERROR: the database '%s' does not have build data" % database_file)
        exit(1)
    fetch_build_data(build[BUILD_ORM_ID])

    while True:
        # get next command
        command = ''
        arg=''
        file=''
        commands = input('Command: ').split()
        for i in range(len(commands)):
            if 0 == i:
                command = commands[i]
                continue
            if '>' == commands[i]:
                if (i+1) < len(commands):
                    file = commands[i+1]
                break;
            arg = commands[i]

        # process commands
        if '?' == command[0]:
            show_help()
            continue
        elif 'q' == command[0]:
            print("Quitting!")
            break
        elif 'b' == command[0]:
            if 0 == len(arg):
                show_builds()
            else:
                if not fetch_build_data(int(arg)):
                    build = None
            continue
        if 0 == len(command):
            continue

        # require build data for the remaining commands
        if None == build:
            print('ERROR: Open a build first to execute this command')
            continue

        if 'd' == command[0]:
            display_statistics(False)
        elif 't' == command[0]:
            display_tasks(arg,False)
        elif 'r' == command[0]:
            display_recipes(arg,False)
        elif 'T' == command[0]:
            display_tasks(arg,True)
        elif 'R' == command[0]:
            display_recipes(arg,True)
        elif 'e' == command[0]:
            display_task_events(arg)
        elif 'E' == command[0]:
            display_recipe_events(arg)
        elif 'o' == command[0]:
            display_task_overlaps(arg,file)
        elif 'O' == command[0]:
            display_recipe_overlaps(arg,file)
        elif 'g' == command[0]:
            graph_task_overlaps(False,arg,file)
        elif 'G' == command[0]:
            graph_recipe_overlaps(False,arg,file)
        elif 'h' == command[0]:
            graph_task_overlaps(True,arg,file)
        elif 'H' == command[0]:
            graph_recipe_overlaps(True,arg,file)

    # clean up and finish
    conn.close()

if __name__ == '__main__':
   main(sys.argv[1:])
