
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#define SENSOR_INVALID_FLOAT (-9999.0f)
#define AD7682_REFERENCE_VOLTS 5.0f
typedef int HAL_StatusTypeDef;
#define HAL_OK 0
#define HAL_ERROR 1
typedef struct
{
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t weekday;
    uint8_t month;
    uint8_t year;
    uint8_t century;
} PCF8563_DateTime;typedef struct
{
    PCF8563_DateTime datetime;
    float temperature_c;
    float humidity_rh;
    float pressure_u7_hpa;
    float pressure_u11_hpa;
    float battery_voltage;
    float adc[4]; /* 滤波后的 ADC 码值；新的 Flash 记录保留 0.1 码精度。 */
} DataLogger_Sample;
static float printed_adc[4];
/* Capture actual ADC arguments passed to formatting. */
static int test_snprintf(char *dest,size_t size,const char *format,...){
  size_t n=strlen(format);unsigned i;va_list args;
  if(strncmp(format,"samples:",8)==0){
    va_start(args,format);
    for(i=0;i<6;i++)(void)va_arg(args,unsigned);
    for(i=0;i<4;i++)(void)va_arg(args,double);
    for(i=0;i<4;i++)printed_adc[i]=(float)va_arg(args,double);
    va_end(args);
  }
  if(n>=size)n=size-1;memcpy(dest,format,n);dest[n]=0;return (int)n;
}
#define snprintf test_snprintf
static uint32_t g_dump_remaining,g_dump_sequence,g_dump_start_time,g_dump_end_time;
static uint32_t g_dump_matched,g_dump_read_errors;
static uint8_t g_dump_newest_first,g_dump_time_filtered,g_dump_hpa,g_rtc_setting_mode=1;
static uint32_t count,last,reads,lines,done,errors,failed,seen[8];
static uint8_t newest;
static uint8_t g_dump_report_pending;
static void Serial_DumpPauseSampling(void){}
static uint32_t g_window_dump_sequence,g_window_dump_remaining,g_window_dump_matched,g_window_dump_errors;
static uint16_t g_window_scanned;
static unsigned window_mode,window_open,window_replay;
static unsigned window_size=16;
static unsigned UartCrc_WindowSize(void){return window_size;}
static int UartCrc_WindowEnabled(void){return window_mode;}
static int UartCrc_WindowOpen(void){return window_open;}
static int UartCrc_TakeReplay(void){int r=window_replay;window_replay=0;return r;}
static void UartCrc_WindowBegin(void){window_open=1;}
static void UartCrc_WindowEnd(void);

static unsigned crc_enabled,crc_busy,crc_failed,crc_sends;
static int UartCrc_IsEnabled(void){return crc_enabled;}
static int UartCrc_IsBusy(void){return crc_busy;}
static int UartCrc_IsFailed(void){return crc_failed;}
static void UartCrc_Send(const char *s){(void)s;crc_sends++;crc_busy=1;}
static void UartCrc_WindowEnd(void){window_open=0;crc_busy=1;}

static DataLogger_Sample rows[8];
static uint32_t DataLogger_Count(void){return count;}
static uint32_t DataLogger_LastSequence(void){return last;}
static uint8_t DataLogger_GetDumpNewestFirst(void){return newest;}
static HAL_StatusTypeDef DataLogger_ReadSequence(uint32_t seq,DataLogger_Sample *s,uint32_t timeout){
  (void)timeout;if(reads<8)seen[reads]=seq;reads++;
  if(seq==failed||seq<10||seq>17)return HAL_ERROR;
  *s=rows[seq-10];return HAL_OK;
}
static void Serial_Write(const char *s){
  if(strncmp(s,"samples:",8)==0){lines++;if(crc_enabled&&!window_open)crc_busy=1;}
  if(strncmp(s,"LOG DUMP DONE:",14)==0)done++;
  if(strncmp(s,"LOG DUMP ERROR:",15)==0)errors++;
}
static uint8_t Serial_ToLower(char c){return (uint8_t)((c>='A'&&c<='Z')?(c+('a'-'A')):c);}
static uint8_t Serial_Equals(const char *a,const char *b){while(*a&&*b&&Serial_ToLower(*a)==Serial_ToLower(*b)){a++;b++;}return (uint8_t)(*a==0&&*b==0);}
static uint8_t Serial_Digits(const char *s,uint8_t n,uint8_t *value){uint8_t i,v=0;if(!s||!value)return 0;for(i=0;i<n;i++){if(s[i]<'0'||s[i]>'9')return 0;v=(uint8_t)(v*10U+(uint8_t)(s[i]-'0'));}*value=v;return 1;}
static uint8_t Serial_IsLeap(uint16_t year){return (uint8_t)((year%4U==0U&&year%100U!=0U)||(year%400U==0U));}
static uint8_t Serial_DaysInMonth(uint16_t year,uint8_t month){static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};if(month==2U&&Serial_IsLeap(year))return 29U;return days[month-1U];}
static uint8_t Serial_Weekday(uint16_t year,uint8_t month,uint8_t day){uint32_t days=0;uint16_t y;uint8_t m;for(y=2000U;y<year;y++)days+=Serial_IsLeap(y)?366U:365U;for(m=1U;m<month;m++)days+=Serial_DaysInMonth(year,m);days+=(uint32_t)(day-1U);return (uint8_t)((6U+days)%7U);}
static uint8_t Serial_ParseSetTime(const char *s,PCF8563_DateTime *dt)
{
  uint8_t yy,mo,da,hh,mi,se;uint16_t year;size_t n;
  if(!s||!dt)return 0;n=strlen(s);
  if(n!=21U||Serial_ToLower(s[0])!='s'||Serial_ToLower(s[1])!='e'||Serial_ToLower(s[2])!='t'||s[3]!=' '||s[6]!='-'||s[9]!='-'||s[12]!=' '||s[15]!=':'||s[18]!=':')return 0;
  if(!Serial_Digits(&s[4],2,&yy)||!Serial_Digits(&s[7],2,&mo)||!Serial_Digits(&s[10],2,&da)||!Serial_Digits(&s[13],2,&hh)||!Serial_Digits(&s[16],2,&mi)||!Serial_Digits(&s[19],2,&se))return 0;
  year=(uint16_t)(2000U+yy);if(mo<1U||mo>12U||da<1U||da>Serial_DaysInMonth(year,mo)||hh>23U||mi>59U||se>59U)return 0;
  dt->year=yy;dt->month=mo;dt->day=da;dt->hours=hh;dt->minutes=mi;dt->seconds=se;dt->weekday=Serial_Weekday(year,mo,da);dt->century=0U;return 1;
}
static uint32_t Serial_DateTimeSeconds(const PCF8563_DateTime *dt)
{
  uint32_t days = 0U;
  uint16_t year;
  uint8_t month;
  if (dt->century != 0U || dt->year > 99U || dt->month < 1U || dt->month > 12U ||
      dt->day < 1U || dt->day > Serial_DaysInMonth((uint16_t)(2000U + dt->year), dt->month) ||
      dt->hours > 23U || dt->minutes > 59U || dt->seconds > 59U)
  {
    return 0xFFFFFFFFUL;
  }
  for (year = 2000U; year < 2000U + dt->year; year++)
  {
    days += Serial_IsLeap(year) ? 366U : 365U;
  }
  for (month = 1U; month < dt->month; month++)
  {
    days += Serial_DaysInMonth((uint16_t)(2000U + dt->year), month);
  }
  days += dt->day - 1U;
  return ((days * 24U + dt->hours) * 60U + dt->minutes) * 60U + dt->seconds;
}
static uint8_t Serial_ParseDumpTime(const char *text, size_t length, uint32_t *seconds)
{
  char command[22] = "SET ";
  PCF8563_DateTime dt;
  if (length == 19U && text[0] == '2' && text[1] == '0')
  {
    text += 2U;
    length -= 2U;
  }
  if (length != 17U) { return 0U; }
  memcpy(command + 4U, text, 17U);
  if (!Serial_ParseSetTime(command, &dt)) { return 0U; }
  *seconds = Serial_DateTimeSeconds(&dt);
  return 1U;
}
static void Serial_PrintDumpResult(void)
{
  char line[80];
  (void)snprintf(line, sizeof(line), "LOG DUMP DONE: matched=%lu read_errors=%lu\r\n",
                 (unsigned long)g_dump_matched, (unsigned long)g_dump_read_errors);
  if (UartCrc_IsEnabled()) { (void)UartCrc_Send(line); }
  else { Serial_Write(line); }
}
static void Serial_StartLogDump(const char *command)
{
  uint32_t start = 0U, end = 0U;
  const char *text = command + 8U;
  uint8_t hpa = 0U, explicit_mode = 0U;
  uint8_t filtered;
  if (UartCrc_IsEnabled() && (UartCrc_IsBusy() || UartCrc_IsFailed() ||
                            g_dump_remaining != 0U || g_dump_report_pending))
  {
    Serial_Write("LOG DUMP ERROR: link busy or failed\r\n");
    return;
  }
  if (*text == ' ') { text++; }
  if (strncmp(text, "hpa", 3U) == 0 && (text[3] == 0 || text[3] == ' '))
  {
    hpa = 1U; explicit_mode = 1U; text += 3U;
    if (*text == ' ') { text++; }
  }
  else if (strncmp(text, "raw", 3U) == 0 && (text[3] == 0 || text[3] == ' '))
  {
    explicit_mode = 1U; text += 3U;
    if (*text == ' ') { text++; }
  }
  filtered = (uint8_t)(*text != 0);
  if (filtered)
  {
    const char *separator = strchr(text, ' ');
    /* 第一个空格分隔日期和时间，第二个空格分隔起止范围。 */
    if (separator != NULL) { separator = strchr(separator + 1U, ' '); }
    if (separator == NULL ||
        !Serial_ParseDumpTime(text, (size_t)(separator - text), &start) ||
        !Serial_ParseDumpTime(separator + 1U, strlen(separator + 1U), &end) ||
        start > end)
    {
      Serial_Write("LOG DUMP ERROR: use log dump [raw|hpa] [YYYY-MM-DD HH:MM:SS YYYY-MM-DD HH:MM:SS] (2000..2099, start <= end)\r\n");
      return;
    }
  }
  /* 先校验，再替换正在进行的导出，随后保存序号快照。 */
  Serial_DumpPauseSampling();
  g_dump_time_filtered = filtered;
  g_dump_hpa = hpa;
  g_dump_start_time = start;
  g_dump_end_time = end;
  g_dump_matched = 0U;
  g_dump_read_errors = 0U;
  g_dump_report_pending = 0U;
  g_dump_remaining = DataLogger_Count();
  g_dump_newest_first = DataLogger_GetDumpNewestFirst();
  g_dump_sequence = g_dump_newest_first ? DataLogger_LastSequence() :
                    DataLogger_LastSequence() - g_dump_remaining + 1U;
  if (explicit_mode)
  {
    Serial_Write(hpa ? "LOG DUMP ADC: hPa (0.5V=350hPa, 4.5V=1350hPa, VREF=5V)\r\n" :
                       "LOG DUMP ADC: codes (stored filtered ADC values)\r\n");
  }
  if (UartCrc_IsEnabled())
  {
    char begin[80];
    (void)snprintf(begin, sizeof(begin), "LOG DUMP BEGIN: scanned=%lu\r\n",
                   (unsigned long)g_dump_remaining);
    Serial_Write(begin);
    if (g_dump_remaining == 0U) { g_dump_report_pending = 1U; }
  }
  else if ((filtered || hpa) && g_dump_remaining == 0U) { Serial_PrintDumpResult(); }
}
static float Serial_AdcPressureHpa(float code)
{
  if (!(code >= 0.0f && code < 65535.0f)) { return SENSOR_INVALID_FLOAT; }
  return code * (AD7682_REFERENCE_VOLTS * 250.0f / 65536.0f) + 225.0f;
}
static void Serial_DumpLogs(void)
{
  uint16_t year;
  char line[180];
  DataLogger_Sample sample;
  uint8_t include;
  if (UartCrc_IsEnabled())
  {
    if (UartCrc_TakeReplay())
    {
      g_dump_sequence = g_window_dump_sequence;
      g_dump_remaining = g_window_dump_remaining;
      g_dump_matched = g_window_dump_matched;
      g_dump_read_errors = g_window_dump_errors;
      g_dump_report_pending = 0U;
      g_window_scanned = 0U;
    }
    if (UartCrc_IsBusy() || UartCrc_IsFailed()) { return; }
    if (g_dump_report_pending)
    {
      g_dump_report_pending = 0U;
      Serial_PrintDumpResult();
      return;
    }
  }

  /* 每次调用只扫描一条；独占主循环持续调用，并在记录之间处理命令和 ACK。
     固定序号快照：导出期间新增记录不混入本次导出。 */
  if (g_dump_remaining != 0U && g_rtc_setting_mode != 0U)
  {
    uint32_t sequence = g_dump_sequence;
    if (UartCrc_WindowEnabled() && !UartCrc_WindowOpen())
    {
      g_window_dump_sequence = g_dump_sequence;
      g_window_dump_remaining = g_dump_remaining;
      g_window_dump_matched = g_dump_matched;
      g_window_dump_errors = g_dump_read_errors;
      g_window_scanned = 0U;
      UartCrc_WindowBegin();
    }
    g_dump_sequence = g_dump_newest_first ? sequence - 1U : sequence + 1U;
    g_dump_remaining--;
    if (DataLogger_ReadSequence(sequence, &sample, 100U) != HAL_OK)
    {
      g_dump_read_errors++;
    }
    else
    {
      include = 1U;
      if (g_dump_time_filtered)
      {
        uint32_t seconds = Serial_DateTimeSeconds(&sample.datetime);
        include = (uint8_t)(seconds >= g_dump_start_time && seconds <= g_dump_end_time);
      }
      if (include)
      {
        if (g_dump_hpa)
        {
          uint8_t channel;
          for (channel = 0U; channel < 4U; channel++)
          {
            sample.adc[channel] = Serial_AdcPressureHpa(sample.adc[channel]);
          }
        }
        year = (uint16_t)(sample.datetime.year +
                         ((sample.datetime.century != 0U) ? 1900U : 2000U));
        (void)snprintf(line, sizeof(line),
                   "samples: %u,%02u,%02u,%02u,%02u,%02u,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\r\n",
                   (unsigned)year, (unsigned)sample.datetime.month,
                   (unsigned)sample.datetime.day, (unsigned)sample.datetime.hours,
                   (unsigned)sample.datetime.minutes, (unsigned)sample.datetime.seconds,
                   (double)sample.temperature_c, (double)sample.humidity_rh,
                   (double)sample.pressure_u7_hpa, (double)sample.pressure_u11_hpa,
                   (double)sample.adc[0], (double)sample.adc[1],
                   (double)sample.adc[2], (double)sample.adc[3],
                   (double)sample.battery_voltage);
        Serial_Write(line);
        g_dump_matched++;
      }
    }
    if (UartCrc_WindowOpen() && (++g_window_scanned >= UartCrc_WindowSize() || g_dump_remaining == 0U))
    { UartCrc_WindowEnd(); }
    if (g_dump_remaining == 0U)
    {
      if (UartCrc_IsEnabled()) { g_dump_report_pending = 1U; }
      else if (g_dump_time_filtered || g_dump_hpa) { Serial_PrintDumpResult(); }
    }
    else if (UartCrc_IsEnabled() && (g_dump_remaining % 128U) == 0U)
    {
      (void)snprintf(line, sizeof(line), "LOG DUMP PROGRESS: remaining=%lu\r\n",
                     (unsigned long)g_dump_remaining);
      Serial_Write(line);
    }
  }
}
#define CHECK(c) do{if(!(c))return __LINE__;}while(0)
static void reset(void){
  unsigned i;window_size=16;window_mode=window_open=window_replay=0;crc_enabled=crc_busy=crc_failed=crc_sends=0;g_dump_report_pending=0;count=5;last=14;reads=lines=done=errors=failed=newest=0;
  g_dump_remaining=0;g_rtc_setting_mode=1;memset(rows,0,sizeof(rows));
  for(i=0;i<8;i++){rows[i].datetime.year=26;rows[i].datetime.month=11;
    rows[i].datetime.day=2;rows[i].datetime.hours=9;rows[i].datetime.seconds=(uint8_t)i;}
}
static int drain(void){unsigned i=0;while(g_dump_remaining&&i++<10)Serial_DumpLogs();return !g_dump_remaining;}
int test_main(void){
  uint32_t seconds;PCF8563_DateTime dt={0};
  CHECK(Serial_ParseDumpTime("2000-01-01 00:00:00",19,&seconds)&&seconds==0);
  CHECK(Serial_ParseDumpTime("2099-12-31 23:59:59",19,&seconds)&&seconds==3155759999UL);
  CHECK(Serial_ParseDumpTime("24-02-29 23:59:59",17,&seconds));
  CHECK(!Serial_ParseDumpTime("2026-02-29 00:00:00",19,&seconds));
  CHECK(!Serial_ParseDumpTime("2026-04-31 00:00:00",19,&seconds));
  CHECK(!Serial_ParseDumpTime("2026-00-01 00:00:00",19,&seconds));
  CHECK(!Serial_ParseDumpTime("2026-13-01 00:00:00",19,&seconds));
  CHECK(!Serial_ParseDumpTime("2026-11-02 24:00:00",19,&seconds));
  CHECK(!Serial_ParseDumpTime("2026-11-02 09:60:00",19,&seconds));
  CHECK(!Serial_ParseDumpTime("2026-11-02 09:00:60",19,&seconds));
  CHECK(!Serial_ParseDumpTime("2100-11-02 09:00:00",19,&seconds));
  CHECK(!Serial_ParseDumpTime("2026/11/02 09:00:00",19,&seconds));
  CHECK(!Serial_ParseDumpTime("",0,&seconds));
  CHECK(Serial_DateTimeSeconds(&dt)==0xFFFFFFFFUL);
  reset();Serial_StartLogDump("log dump 2026-11-02 09:00:01 2026-11-02 09:00:03");
  CHECK(drain()&&reads==5&&lines==3&&done==1&&g_dump_matched==3);
  CHECK(seen[0]==10&&seen[4]==14);
  reset();newest=1;Serial_StartLogDump("log dump 26-11-02 09:00:01 26-11-02 09:00:03");
  CHECK(drain()&&lines==3&&seen[0]==14&&seen[4]==10);
  reset();rows[2].datetime.seconds=1;
  Serial_StartLogDump("log dump 2026-11-02 09:00:01 26-11-02 09:00:01");
  CHECK(drain()&&lines==2&&done==1);
  reset();Serial_StartLogDump("log dump 2026-11-01 23:59:59 2026-11-02 09:00:01");
  CHECK(drain()&&lines==2);
  reset();Serial_StartLogDump("log dump 2026-11-03 00:00:00 2026-11-03 23:59:59");
  CHECK(drain()&&lines==0&&done==1&&!g_dump_read_errors);
  reset();count=0;Serial_StartLogDump("log dump 26-11-02 09:00:00 26-11-02 09:00:01");
  CHECK(done==1&&!reads);
  reset();Serial_StartLogDump("log dump");
  Serial_StartLogDump("log dump 26-11-02 09:00:02 26-11-02 09:00:01");
  CHECK(errors==1&&g_dump_remaining==5&&!g_dump_time_filtered);
  Serial_StartLogDump("log dump nonsense");Serial_StartLogDump("log dump invalid");
  Serial_StartLogDump("log dump 2026-11-02 09:00:00");
  Serial_StartLogDump("log dump 26-11-02 09:00:00 26-11-02 09:00:01 junk");
  CHECK(errors==5&&drain()&&lines==5&&done==0);
  reset();failed=14;Serial_StartLogDump("log dump 26-11-02 09:00:00 26-11-02 09:00:04");
  CHECK(drain()&&lines==4&&done==1&&g_dump_read_errors==1);
  reset();rows[0].datetime.hours=10;rows[4].datetime.hours=8;
  Serial_StartLogDump("log dump 26-11-02 09:00:00 26-11-02 09:00:04");count=6;last=15;
  CHECK(drain()&&reads==5&&lines==3);
  reset();rows[2].datetime.month=15;
  Serial_StartLogDump("log dump 26-11-02 09:00:00 26-11-02 09:00:04");CHECK(drain()&&lines==4);
  reset();Serial_StartLogDump("log dump 26-11-02 09:00:00 26-11-02 09:00:04");
  Serial_DumpLogs();g_dump_remaining=0;Serial_DumpLogs();CHECK(lines==1&&reads==1&&!done);
  Serial_StartLogDump("log dump");CHECK(drain()&&!g_dump_time_filtered&&lines==6&&!done);

  CHECK(Serial_AdcPressureHpa(0.0f)==225.0f);
  CHECK(Serial_AdcPressureHpa(6553.6f)>349.99f&&Serial_AdcPressureHpa(6553.6f)<350.01f);
  CHECK(Serial_AdcPressureHpa(19660.8f)>599.99f&&Serial_AdcPressureHpa(19660.8f)<600.01f);
  CHECK(Serial_AdcPressureHpa(39321.6f)>974.99f&&Serial_AdcPressureHpa(39321.6f)<975.01f);
  CHECK(Serial_AdcPressureHpa(58982.4f)>1349.99f&&Serial_AdcPressureHpa(58982.4f)<1350.01f);
  CHECK(Serial_AdcPressureHpa(65534.9f)>1474.9f);
  CHECK(Serial_AdcPressureHpa(65535.0f)==-9999.0f);
  CHECK(Serial_AdcPressureHpa(-9999.0f)==-9999.0f);
  CHECK(Serial_AdcPressureHpa(100000.0f)==-9999.0f);
  reset();count=1;last=10;
  rows[0].adc[0]=6553.6f;rows[0].adc[1]=19660.8f;
  rows[0].adc[2]=39321.6f;rows[0].adc[3]=58982.4f;
  Serial_StartLogDump("log dump hpa");
  CHECK(g_dump_hpa&&!g_dump_time_filtered&&drain()&&lines==1&&done==1);
  CHECK(printed_adc[0]>349.99f&&printed_adc[0]<350.01f);
  CHECK(printed_adc[1]>599.99f&&printed_adc[1]<600.01f);
  CHECK(printed_adc[2]>974.99f&&printed_adc[2]<975.01f);
  CHECK(printed_adc[3]>1349.99f&&printed_adc[3]<1350.01f);
  CHECK(rows[0].adc[0]==6553.6f);
  Serial_StartLogDump("log dump raw");CHECK(!g_dump_hpa&&drain()&&printed_adc[0]==6553.6f);
  Serial_StartLogDump("log dump hpa");Serial_StartLogDump("log dump");CHECK(!g_dump_hpa&&drain());
  reset();newest=1;
  Serial_StartLogDump("log dump hpa 2026-11-02 09:00:01 2026-11-02 09:00:03");
  CHECK(g_dump_hpa&&g_dump_time_filtered&&drain()&&lines==3&&done==1&&seen[0]==14);
  reset();Serial_StartLogDump("log dump raw 26-11-02 09:00:01 26-11-02 09:00:03");
  CHECK(!g_dump_hpa&&g_dump_time_filtered&&drain()&&lines==3&&done==1);
  reset();count=0;Serial_StartLogDump("log dump hpa");CHECK(done==1&&!reads);
  reset();Serial_StartLogDump("log dump hpa");
  Serial_StartLogDump("log dump invalid");CHECK(errors==1&&g_dump_hpa&&g_dump_remaining==5);
  Serial_StartLogDump("log dump hpax");CHECK(errors==2&&g_dump_hpa);
  Serial_StartLogDump("log dump hpa 2026-11-02 09:00:00");CHECK(errors==3&&g_dump_hpa);
  reset();crc_enabled=1;Serial_StartLogDump("log dump raw");
  CHECK(g_dump_remaining==5);Serial_DumpLogs();CHECK(lines==1&&crc_busy&&g_dump_remaining==4);
  Serial_DumpLogs();CHECK(lines==1&&g_dump_remaining==4);
  while(g_dump_remaining){crc_busy=0;Serial_DumpLogs();}
  CHECK(lines==5&&g_dump_report_pending&&crc_sends==0);
  Serial_DumpLogs();CHECK(crc_sends==0); /* Last row still awaiting ACK. */
  crc_busy=0;Serial_DumpLogs();CHECK(crc_sends==1&&crc_busy&&!g_dump_report_pending);
  reset();crc_enabled=1;count=0;Serial_StartLogDump("log dump raw");
  CHECK(g_dump_report_pending&&crc_sends==0);Serial_DumpLogs();CHECK(crc_sends==1);
  reset();crc_enabled=1;Serial_StartLogDump("log dump raw");crc_failed=1;
  Serial_DumpLogs();CHECK(!reads&&g_dump_remaining==5);
  reset();crc_enabled=window_mode=1;Serial_StartLogDump("log dump raw");
  CHECK(drain()&&lines==5&&g_dump_matched==5&&crc_busy&&g_dump_report_pending);
  window_replay=1;crc_busy=0;Serial_DumpLogs();
  CHECK(lines==6&&g_dump_matched==1&&g_dump_remaining==4&&!g_dump_report_pending);
  CHECK(drain()&&lines==10&&g_dump_matched==5&&g_dump_read_errors==0);
  crc_busy=0;Serial_DumpLogs();CHECK(crc_sends==1);
  reset();crc_enabled=window_mode=1;newest=1;
  Serial_StartLogDump("log dump raw 2026-11-02 09:00:01 2026-11-02 09:00:03");
  failed=12;CHECK(drain()&&g_dump_matched==2&&g_dump_read_errors==1);
  window_replay=1;crc_busy=0;Serial_DumpLogs();CHECK(drain());
  CHECK(g_dump_matched==2&&g_dump_read_errors==1&&seen[0]==14);
  reset();crc_enabled=window_mode=1;window_size=1000;count=1001;last=1010;
  Serial_StartLogDump("log dump raw");
  {unsigned j;for(j=0;j<999;j++)Serial_DumpLogs();}
  CHECK(g_window_scanned==999&&!crc_busy&&g_dump_remaining==2);
  Serial_DumpLogs();CHECK(crc_busy&&g_window_scanned==1000&&g_dump_remaining==1);
  crc_busy=0;Serial_DumpLogs();CHECK(crc_busy&&g_dump_remaining==0&&g_dump_report_pending);
  CHECK(g_dump_read_errors==993&&g_dump_matched==8);
  return 0;
}
