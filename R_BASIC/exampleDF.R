nmbr = c(2, 3, 5) 
smbl = c("aa", "bb", "cc") 
bl = c(TRUE, FALSE, TRUE) 
df1 = data.frame(nmbr,smbl, bl) 
# df - ����� ������
head(df1)

tail(df1)


#�������� �������� id, name, age and country are vectors
#Data Frame Creation
#������ id column, fill 1 to 3

  id <- c ( 1 : 3 )
#������ name

  name <- c ( "Janice" , "Tommy" , "Brad" )
#������ �������

  age <- c ( 23 , 31 , 25 )
#������
country <- c ( "USA" , "Canada" , "Argentina" )
#�������
person_df <- data.frame ( id, name, age, country )
#�������� data frame
print ( person_df )

print (person_df[c(1,3),c(1,3)])
