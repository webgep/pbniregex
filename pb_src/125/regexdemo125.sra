$PBExportHeader$regexdemo125.sra
$PBExportComments$Generated Application Object
forward
global type regexdemo125 from application
end type
global transaction sqlca
global dynamicdescriptionarea sqlda
global dynamicstagingarea sqlsa
global error error
global message message
end forward

global type regexdemo125 from application
string appname = "regexdemo125"
end type
global regexdemo125 regexdemo125

on regexdemo125.create
appname="regexdemo125"
message=create message
sqlca=create transaction
sqlda=create dynamicdescriptionarea
sqlsa=create dynamicstagingarea
error=create error
end on

on regexdemo125.destroy
destroy(sqlca)
destroy(sqlda)
destroy(sqlsa)
destroy(error)
destroy(message)
end on

event open;
open(w_test)

end event

