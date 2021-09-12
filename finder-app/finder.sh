#!/bin/sh

echo running finder script
echo ===============================

#comparing number of arguments passed through commandline
if [ $# -ne 2 ] 
then
	echo "Incorrect number of parameters (expected 2 : <filesdir> <searchstr>)"
	exit 1
fi

filesdir=$1
searchstr=$2

#checking whether directory exists
if [ ! -d "$filesdir" ]
then
	echo "$filesdir : Directory does not exist"
	exit 1
fi

#echo $filesdir

#searching fo a strings within directory and counting the files with matches
nooffiles=$(grep -lr $searchstr $filesdir | wc -l)
#nooffiles=$(find $filesdir -type f | wc -l)
#echo $nooffiles

filesdir=$1
searchstr=$2
# grep -rh "echo" $filesdir

#counting the lines with matches
no_of_matches_per_file=$(grep -rch $searchstr $filesdir)
#echo $no_of_matches_per_file

noofmatchinglines=0;

for match_in_file in $no_of_matches_per_file
do
	# echo $match_in_file
	noofmatchinglines=$((match_in_file + noofmatchinglines))
done

echo "The number of files are $nooffiles and the number of matching lines are $noofmatchinglines"
