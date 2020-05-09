

-- July 02 2013  roughly 1:20 ??

--8467:   2113  33  19
--513:    0201  2  1
--3335:   0d07  13  7


x = 8467
y = 513
z = 3335

xS = string.format("%04x",x)
yS = string.format("%04x",y)
zS = string.format("%04x",z)


X1 = string.sub(xS,1,2)
X2 = string.sub(xS,3,4)
Y1 = string.sub(yS,1,2)
Y2 = string.sub(yS,3,4)
Z1 = string.sub(zS,1,2)
Z2 = string.sub(zS,3,4)


print(xS .. '  ' .. tonumber(X1,16) .. '  ' ..  tonumber(X2,16))
print(yS .. '  ' .. tonumber(Y1,16) .. '  ' ..  tonumber(Y2,16))
print(zS .. '  ' .. tonumber(Z1,16) .. '  ' ..  tonumber(Z2,16))



print(string.format("%02d/%02d/20%02d  %02d:%02d:%02d",tonumber(Z2,16),tonumber(Y1,16),tonumber(Z1,16),
                                                       tonumber(Y2,16),tonumber(X2,16),tonumber(X1,16)))
