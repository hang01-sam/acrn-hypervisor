/*
 * Copyright (c) 2014, Neel Natu (neel@freebsd.org)
 * Copyright (c) 2022 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <asm/guest/vm.h>
#include <asm/io.h>
#include <asm/tsc.h>
#include <vrtc.h>
#include <logmsg.h>

#include "mc146818rtc.h"

/* #define DEBUG_RTC */
#ifdef DEBUG_RTC
# define RTC_DEBUG  pr_info
#else
# define RTC_DEBUG(format, ...)      do { } while (false)
#endif

struct clktime {
	uint32_t	year;	/* year (4 digit year) */
	uint32_t	mon;	/* month (1 - 12) */
	uint32_t	day;	/* day (1 - 31) */
	uint32_t	hour;	/* hour (0 - 23) */
	uint32_t	min;	/* minute (0 - 59) */
	uint32_t	sec;	/* second (0 - 59) */
	uint32_t	dow;	/* day of week (0 - 6; 0 = Sunday) */
};

#define POSIX_BASE_YEAR	1970
#define SECDAY		(24 * 60 * 60)
#define SECYR		(SECDAY * 365)
#define VRTC_BROKEN_TIME	((time_t)-1)

#define FEBRUARY	2U

static const uint32_t month_days[12] = {
	31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U
};

/*
 * This inline avoids some unnecessary modulo operations
 * as compared with the usual macro:
 *   ( ((year % 4) == 0 &&
 *      (year % 100) != 0) ||
 *     ((year % 400) == 0) )
 * It is otherwise equivalent.
 */
static inline uint32_t leapyear(uint32_t year)
{
	uint32_t rv = 0U;

	if ((year & 3U) == 0) {
		rv = 1U;
		if ((year % 100U) == 0) {
			rv = 0U;
			if ((year % 400U) == 0) {
				rv = 1U;
			}
		}
	}
	return rv;
}

static inline uint32_t days_in_year(uint32_t year)
{
        return leapyear(year) ? 366U : 365U;
}

static inline uint32_t days_in_month(uint32_t year, uint32_t month)
{
        return month_days[(month) - 1U] + ((month == FEBRUARY) ? leapyear(year) : 0U);
}

/*
 * Day of week. Days are counted from 1/1/1970, which was a Thursday.
 */
static inline uint32_t day_of_week(uint32_t days)
{
        return ((days) + 4U) % 7U;
}

/*
 * Translate clktime (such as year, month, day) to time_t.
 */
static int32_t clk_ct_to_ts(struct clktime *ct, time_t *sec)
{
	uint32_t i, year, days;
	int32_t err = 0;

	year = ct->year;

	/* Sanity checks. */
	if ((ct->mon < 1U) || (ct->mon > 12U) || (ct->day < 1U) ||
			(ct->day > days_in_month(year, ct->mon)) ||
			(ct->hour > 23U) ||  (ct->min > 59U) || (ct->sec > 59U) ||
			(year < POSIX_BASE_YEAR) || (year > 2037U)) {
		/* time_t overflow */
		err = -EINVAL;
	} else {
		/*
		 * Compute days since start of time
		 * First from years, then from months.
		 */
		days = 0U;
		for (i = POSIX_BASE_YEAR; i < year; i++) {
			days += days_in_year(i);
		}

		/* Months */
		for (i = 1; i < ct->mon; i++) {
			days += days_in_month(year, i);
		}
		days += (ct->day - 1);

		*sec = (((time_t)days * 24 + ct->hour) * 60 + ct->min) * 60 + ct->sec;
	}
	return err;
}

/*
 * Translate time_t to clktime (such as year, month, day)
 */
static int32_t clk_ts_to_ct(time_t secs, struct clktime *ct)
{
	uint32_t i, year, days;
	time_t rsec;	/* remainder seconds */
	int32_t err = 0;

	days = secs / SECDAY;
	rsec = secs % SECDAY;

	ct->dow = day_of_week(days);

	/* Substract out whole years, counting them in i. */
	for (year = POSIX_BASE_YEAR; days >= days_in_year(year); year++) {
		days -= days_in_year(year);
	}
	ct->year = year;

	/* Substract out whole months, counting them in i. */
	for (i = 1; days >= days_in_month(year, i); i++) {
		days -= days_in_month(year, i);
	}
	ct->mon = i;

	/* Days are what is left over (+1) from all that. */
	ct->day = days + 1;

	/* Hours, minutes, seconds are easy */
	ct->hour = rsec / 3600U;
	rsec = rsec % 3600U;
	ct->min  = rsec / 60U;
	rsec = rsec % 60U;
	ct->sec  = rsec;

	/* time_t is defined as int32_t, so year should not be more than 2037. */
	if ((ct->mon > 12U) || (ct->year > 2037) || (ct->day > days_in_month(ct->year, ct->mon))) {
		pr_err("Invalid vRTC param mon %d, year %d, day %d\n", ct->mon, ct->year, ct->day);
		err = -EINVAL;
	}
	return err;
}

/*
 * Calculate second value from rtcdev register info which save in vrtc.
 */
static time_t rtc_to_secs(const struct acrn_vrtc *vrtc)
{
	struct clktime ct;
	time_t second = VRTC_BROKEN_TIME;
	const struct rtcdev *rtc= &vrtc->rtcdev;

	do {
		ct.sec = rtc->sec;
		ct.min = rtc->min;
		ct.hour = rtc->hour;
		ct.day = rtc->day_of_month;
		ct.mon = rtc->month;

		/*
		 * Ignore 'rtc->dow' because some guests like Linux don't bother
		 * setting it at all while others like OpenBSD/i386 set it incorrectly.
		 *
		 * clock_ct_to_ts() does not depend on 'ct.dow' anyways so ignore it.
		 */
		ct.dow = -1;

		ct.year = rtc->century * 100 + rtc->year;
		if (ct.year < POSIX_BASE_YEAR) {
			pr_err("Invalid RTC century %x/%d\n", rtc->century,
					ct.year);
			break;
		}

		if (clk_ct_to_ts(&ct, &second) != 0) {
			pr_err("Invalid RTC clocktime.date %04d-%02d-%02d\n",
					ct.year, ct.mon, ct.day);
			pr_err("Invalid RTC clocktime.time %02d:%02d:%02d\n",
					ct.hour, ct.min, ct.sec);
			break;
		}
	} while (false);

	return second;
}

/*
 * Translate second value to rtcdev register info and save it in vrtc.
 */
static void secs_to_rtc(time_t rtctime, struct acrn_vrtc *vrtc)
{
	struct clktime ct;
	struct rtcdev *rtc;

	if ((rtctime > 0) && (clk_ts_to_ct(rtctime, &ct) == 0)) {
		rtc = &vrtc->rtcdev;
		rtc->sec = ct.sec;
		rtc->min = ct.min;
		rtc->hour = ct.hour;
		rtc->day_of_week = ct.dow + 1;
		rtc->day_of_month = ct.day;
		rtc->month = ct.mon;
		rtc->year = ct.year % 100;
		rtc->century = ct.year / 100;
	}
}

/*
 * If the base_rtctime is valid, calculate current time by add tsc offset and offset_rtctime.
 */
static time_t vrtc_get_current_time(struct acrn_vrtc *vrtc)
{
	uint64_t offset;
	time_t second = VRTC_BROKEN_TIME;

	if (vrtc->base_rtctime > 0) {
		offset = (cpu_ticks() - vrtc->base_tsc) / (get_tsc_khz() * 1000U);
		second = vrtc->base_rtctime + (time_t)offset;
	}
	return second;
}

#define CMOS_ADDR_PORT		0x70U
#define CMOS_DATA_PORT		0x71U

static spinlock_t cmos_lock = { .head = 0U, .tail = 0U };

static uint8_t cmos_read(uint8_t addr)
{
	pio_write8(addr, CMOS_ADDR_PORT);
	return pio_read8(CMOS_DATA_PORT);
}

static bool cmos_update_in_progress(void)
{
	return (cmos_read(RTC_STATUSA) & RTCSA_TUP) ? 1 : 0;
}

static uint8_t cmos_get_reg_val(uint8_t addr)
{
	uint8_t reg;
	int32_t tries = 2000;

	spinlock_obtain(&cmos_lock);

	/* Make sure an update isn't in progress */
	while (cmos_update_in_progress() && (tries != 0)) {
		tries -= 1;
	}

	reg = cmos_read(addr);

	spinlock_release(&cmos_lock);
	return reg;
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool vrtc_read(struct acrn_vcpu *vcpu, uint16_t addr, __unused size_t width)
{
	uint8_t offset;
	time_t current;
	struct acrn_vrtc *vrtc = &vcpu->vm->vrtc;
	struct acrn_pio_request *pio_req = &vcpu->req.reqs.pio_request;
	struct acrn_vm *vm = vcpu->vm;
	bool ret = true;

	offset = vm->vrtc.addr;

	if (addr == CMOS_ADDR_PORT) {
		pio_req->value = offset;
	} else {
		if (is_service_vm(vm)) {
			pio_req->value = cmos_get_reg_val(offset);
		} else {
			if (offset <= RTC_CENTURY) {
				current = vrtc_get_current_time(vrtc);
				secs_to_rtc(current, vrtc);

				pio_req->value = *((uint8_t *)&vm->vrtc.rtcdev + offset);
				RTC_DEBUG("read 0x%x, 0x%x", offset, pio_req->value);
			} else {
				pr_err("vrtc read invalid addr 0x%x", offset);
				ret = false;
			}
		}
	}

	return ret;
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool vrtc_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t width,
			uint32_t value)
{
	if ((width == 1U) && (addr == CMOS_ADDR_PORT)) {
		vcpu->vm->vrtc.addr = (uint8_t)value & 0x7FU;
	}

	return true;
}

static void vrtc_set_basetime(struct acrn_vrtc *vrtc)
{
	struct rtcdev *vrtcdev = &vrtc->rtcdev;

	/*
	 * Read base time from physical rtc.
	 */
	vrtcdev->sec = cmos_get_reg_val(RTC_SEC);
	vrtcdev->min = cmos_get_reg_val(RTC_MIN);
	vrtcdev->hour = cmos_get_reg_val(RTC_HRS);
	vrtcdev->day_of_month = cmos_get_reg_val(RTC_DAY);
	vrtcdev->month = cmos_get_reg_val(RTC_MONTH);
	vrtcdev->year = cmos_get_reg_val(RTC_YEAR);
	vrtcdev->century = cmos_get_reg_val(RTC_CENTURY);
	vrtcdev->reg_a = cmos_get_reg_val(RTC_STATUSA) & (~RTCSA_TUP);
	vrtcdev->reg_b = cmos_get_reg_val(RTC_STATUSB);
	vrtcdev->reg_c = cmos_get_reg_val(RTC_INTR);
	vrtcdev->reg_d = cmos_get_reg_val(RTC_STATUSD);

	vrtc->base_rtctime = rtc_to_secs(vrtc);
}

void vrtc_init(struct acrn_vm *vm)
{
	struct vm_io_range range = {
	.base = CMOS_ADDR_PORT, .len = 2U};

	/* Initializing the CMOS RAM offset to 0U */
	vm->vrtc.addr = 0U;

	vm->vrtc.vm = vm;
	register_pio_emulation_handler(vm, RTC_PIO_IDX, &range, vrtc_read, vrtc_write);

	vrtc_set_basetime(&vm->vrtc);
	vm->vrtc.base_tsc = cpu_ticks();
}
