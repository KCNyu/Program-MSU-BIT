# N ��������� �����
# xts - ������� �������������
set.seed(16)
N<-10
data <- rnorm(N)

# dates ��� Date ����� � ������� 2016-01-01
dates <- seq(as.Date("2021-01-01"), length = N, by = "days")

# ������� ������ xts() 
ts1 <- xts(x = data, order.by = dates)


ts2 <- xts(x = data, order.by = dates)

# Extract the core data of ts2
ts2_core <- coredata(ts2)
ts2_core
# class 
class(ts2_core)

# ������ ts2
ts2_index <- index(ts2)
ts2_index
# ����� ts2_index
class(ts2_index)

plot(ts2)
ts3<-ts1+0.2*ts2^2

plot(ts3)