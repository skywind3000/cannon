//=====================================================================
//
// TML <Transmod Library>, by skywind 2004, itranspoc.h
//
// HISTORY:
// Dec. 25 2004   skywind  created and implement tcp operation
// Aug. 19 2005   skywind  implement udp operation
// Oct. 27 2005   skywind  interface add set nodelay in SYSCD
// Nov. 25 2005   skywind  extend connection close status
// Dec. 02 2005   skywind  implement channel timer event
// Dec. 17 2005   skywind  implement ioctl event
// Apr. 27 2010   skywind  fixed: when sys-timer changed, maybe error
// Mar. 15 2011   skywind  64bit support, header size configurable
// Jun. 25 2011   skywind  implement channel subscribe
// Sep. 09 2011   skywind  new: socket buf resize, congestion ctrl.
// Nov. 30 2011   skywind  new: channel broadcasting (v2.40)
// Dec. 23 2011   skywind  new: rc4 crypt (v2.43)
// Dec. 28 2011   skywind  rc4 enchance (v2.44)
//
// NOTES： 
// 网络传输库 TML<传输模块>，建立 客户/频道的通信模式，提供基于多频道
// multi-channel通信的 TCP/UDP通信机制，缓存/内存管理，超时控制等服务
//
//=====================================================================

#include "itransmod.h"


//=====================================================================
// Event Handlers
//=====================================================================


//---------------------------------------------------------------------
// 处理接收连接事件
//---------------------------------------------------------------------
int itm_event_accept(int hmode) 
{
	struct sockaddr remote4;
	struct sockaddr_in *addr4;
#ifdef AF_INET6
	struct sockaddr_in6 addr6;
#endif
	struct ITMD *itmd, *channel;
	int sock = -1, node = 0, retval;
	int r1, r2, r3, result = 0;
	unsigned long noblock = 1;
	unsigned long revalue = 1;
	unsigned long bufsize = 0;
	long sockrcv, socksnd;
	int addrlen = 0;
	char *epname;
	
	// 判断套接字并接受连接
	if (hmode == ITMD_OUTER_HOST4) {
		sock = apr_accept(itm_outer_sock4, &remote4, NULL);
		epname = itm_epname4(&remote4);
	}
	else if (hmode == ITMD_INNER_HOST4) {
		sock = apr_accept(itm_inner_sock4, &remote4, NULL);
		epname = itm_epname4(&remote4);
	}
#ifdef AF_INET6
	else if (hmode == ITMD_OUTER_HOST6) {
		addrlen = sizeof(addr6);
		sock = apr_accept(itm_outer_sock6, (struct sockaddr*)&addr6, &addrlen);
		epname = itm_epname6((struct sockaddr*)&addr6);
	}
	else if (hmode == ITMD_INNER_HOST6) {
		addrlen = sizeof(addr6);
		sock = apr_accept(itm_inner_sock6, (struct sockaddr*)&addr6, &addrlen);
		epname = itm_epname6((struct sockaddr*)&addr6);
	}
#else
	addrlen = addrlen;
#endif

	// 处理失败
	if (sock < 0) {
		static apr_int64 last_log_time = 0;		// 判断时间避免疯狂刷日志
		apr_int64 current = apr_timex() / 1000;
		if (current - last_log_time >= 100) {
			itm_log(ITML_ERROR, "[ERROR] can not accept new %s connection errno=%d", 
				ITMD_HOST_IS_OUTER(hmode)? "user" : "channel", apr_errno());
			last_log_time = current;
		}
		return -1;
	}

	apr_enable(sock, APR_CLOEXEC);

	if (ITMD_HOST_IS_OUTER(hmode)) {
		if (itm_outer_cnt >= itm_outer_max) {
			apr_close(sock);
			itm_log(ITML_ERROR, 
				"[ERROR] connection refused: max connection riched limit %d", 
				itm_outer_max);
			return -2;
		}
	}	else {
		if (itm_inner_cnt >= itm_inner_max) {
			apr_close(sock);
			itm_log(ITML_ERROR, 
				"[ERROR] connection refused: max connection riched limit %d", 
				itm_inner_max);
			return -2;
		}
		if (itm_validate) {
			if (ITMD_HOST_IS_IPV4(hmode)) {
				retval = itm_validate(&remote4);
			}	else {
			#ifdef AF_INET6
				retval = itm_validate((struct sockaddr*)&addr6);
			#endif
			}
			if (retval != 0) {
				apr_close(sock);
				itm_log(ITML_ERROR, 
					"[ERROR] connection refused: invalid ip for %s code=%d",
					epname, retval);
				return -2;
			}
		}
	}

	node = imp_newnode(&itm_fds);
	if (node < 0 || node >= 0x10000) {
		apr_close(sock);
		if (node >= 0) imp_delnode(&itm_fds, node);
		itm_log(ITML_ERROR, 
			"[ERROR] connection refused: error allocate connection descripter");
		return -3;
	}

	apr_ioctl(sock, FIONBIO, &noblock);
	apr_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&revalue, sizeof(revalue));

	if (hmode == ITMD_OUTER_HOST4 || hmode == ITMD_OUTER_HOST6) {
		socksnd = itm_socksndo;
		sockrcv = itm_sockrcvo;
	}	else {
		socksnd = itm_socksndi;
		sockrcv = itm_sockrcvi;
	}

	if (socksnd > 0) {
		bufsize = (unsigned long)socksnd;
		apr_setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&bufsize, sizeof(bufsize));
	}

	if (sockrcv > 0) {
		bufsize = (unsigned long)sockrcv;
		apr_setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&bufsize, sizeof(bufsize));
	}

	itmd = (struct ITMD*)IMP_DATA(&itm_fds, node);
	memset(itmd, 0, sizeof(struct ITMD));

	if (hmode == ITMD_OUTER_HOST4) {
		itmd->mode = ITMD_OUTER_CLIENT;
		itmd->IsIPv6 = 0;
	}
	else if (hmode == ITMD_OUTER_HOST6) {
		itmd->mode = ITMD_OUTER_CLIENT;
		itmd->IsIPv6 = 1;
	}
	else if (hmode == ITMD_INNER_HOST4) {
		itmd->mode = ITMD_INNER_CLIENT;
		itmd->IsIPv6 = 0;
	}
	else if (hmode == ITMD_INNER_HOST6) {
		itmd->mode = ITMD_INNER_CLIENT;
		itmd->IsIPv6 = 1;
	}

	itmd->node = node;
	itmd->fd = sock;
	itmd->tag = -1;
	itmd->mask = APOLL_ERR;
	itmd->hid = (((++itm_counter) & 0x7fff) << 16) | (node & 0xffff);
	itmd->channel = ITMD_HOST_IS_OUTER(hmode)? 0 : -1;
	itmd->timeid = -1;
	itmd->initok = 0;
	itmd->ccnum = 0;
	itmd->inwlist = 0;
	itmd->disable = 0;

	#ifndef IDISABLE_RC4
	itmd->rc4_send_x = -1;
	itmd->rc4_send_y = -1;
	itmd->rc4_recv_x = -1;
	itmd->rc4_recv_y = -1;
	#endif

	if (ITMD_HOST_IS_OUTER(hmode)) itm_outer_cnt++;
	else itm_inner_cnt++;

	if (itmd->IsIPv6 == 0) {
		memcpy(&(itmd->remote4), &remote4, sizeof(remote4));
	}	else {
	#ifdef AF_INET6
		memcpy(&(itmd->remote6), &addr6, sizeof(addr6));
	#endif
	}

	r1 = ims_init(&itmd->rstream, &itm_mem);
	r2 = ims_init(&itmd->wstream, &itm_mem);
	r3 = ims_init(&itmd->lstream, &itm_mem);

	itmd->timeid = ITMD_HOST_IS_OUTER(hmode)? 
					idt_newtime(&itm_timeu, itmd->hid) : idt_newtime(&itm_timec, itmd->hid);

	if (itmd->timeid < 0 || r1 || r2 || r3) {
		itm_log(ITML_ERROR, "[ERROR] memory stream or collection set error for %s", epname);
		itm_event_close(itmd, 4001);
		return 0;
	}

	retval = apr_poll_add(itm_polld, itmd->fd, APOLL_IN | APOLL_ERR, itmd);

	if (retval) {
		itm_log(ITML_ERROR, "[ERROR] poll add fd %d error for %s", 
			itmd->fd, epname);
		itm_event_close(itmd, 4000);
		return 0;
	}

	iv_queue(&itmd->waitq);
	iv_qnode(&itmd->wnode, itmd);
	itm_mask(itmd, ITM_READ, 0);

	itmd->session = (unsigned long)((itm_wtime << 16) | itm_counter);
	itmd->touched = 0;
	itmd->dropped = 0;
	itmd->cnt_tcpr = 0;
	itmd->cnt_tcpw = 0;
	itmd->cnt_udpr = 0;
	itmd->cnt_udpw = 0;
	itmd->skipped = 0;
	itmd->history1 = 0;
	itmd->history2 = 0;
	itmd->history3 = 0;
	itmd->history4 = 0;
	itmd->initok = 1;

	if (hmode == ITMD_OUTER_HOST4 || hmode == ITMD_OUTER_HOST6) {
		int addrsize = 6;
		channel = (struct ITMD*)itm_rchannel(0);
		if (channel == NULL) {
			itm_log(ITML_ERROR, "[ERROR] connection refused: channel 0 does not exist");
			itm_event_close(itmd, 2300);
		}
		else if (channel->wstream.size > itm_inner_blimit) {
			itm_log(ITML_ERROR, "[ERROR] connection refused: channel 0 is too busy");
			itm_event_close(itmd, 2301);
		}
		else {
			channel->ccnum++;
			if (itmd->IsIPv6 == 0) {
				itm_param_set(0, itm_headlen + 6, ITMT_NEW, itmd->hid, itmd->tag);
				addr4 = (struct sockaddr_in*)&remote4;
				memcpy(itm_data + itm_headlen, &addr4->sin_addr.s_addr, 4);
				itm_write_word(itm_data + itm_headlen + 4, (unsigned short)(addr4->sin_port));
			}	else {
				addrsize = 16 + 2;
				itm_param_set(0, itm_headlen + addrsize, ITMT_NEW, itmd->hid, itmd->tag);
			#ifdef AF_INET6
				memcpy(itm_data + itm_headlen, &addr6.sin6_addr.s6_addr, 16);
				itm_write_word(itm_data + itm_headlen + 16, (unsigned short)(addr6.sin6_port));
			#endif
			}
			itm_send(channel, itm_data, itm_headlen + addrsize);
			itm_log(ITML_INFO, "new user connected hid=%XH from %s", itmd->hid, epname);
			result = 1;
		}
		if (itm_booklen[0] > 0 && result == 1) {
			long len, i;
			len = itm_booklen[0];
			for (i = 0; i < len; i++) {
				int chid = itm_book[0][i];
				channel = (struct ITMD*)itm_rchannel(chid);
				if (channel != NULL && chid != 0) {
					itm_send(channel, itm_data, itm_headlen + addrsize);
				}
			}
		}
	}	else {
		result = 2;
		itm_log(ITML_INFO, "new channel connected from %s", epname);
	}

	// 如果UDP开启则发送TOUCH信号
	if ((itm_udpmask & ITMU_MWORK) && result == 1) {
		char *ptr = itm_data + itm_hdrsize;
		itm_size_set(itm_data, itm_hdrsize + 14);
		iencode16u_lsb(ptr +  0, ITMU_TOUCH);
		iencode32u_lsb(ptr +  2, itmd->hid);
		iencode32u_lsb(ptr +  6, itmd->session);
		iencode16u_lsb(ptr + 10, (unsigned short)itm_udpmask);
		iencode16u_lsb(ptr + 12, (unsigned short)itm_dgram_port4);
	#ifdef AF_INET6
		if (itmd->IsIPv6) {
			iencode16u_lsb(ptr + 12, (unsigned short)itm_dgram_port6);
		}
	#endif
		itm_send(itmd, itm_data, itm_hdrsize + 14);
	}

	return 0;
}

//---------------------------------------------------------------------
// 处理断开事件
//---------------------------------------------------------------------
int itm_event_close(struct ITMD *itmd, int code)
{
	struct ITMD *channel;
	long mode;

	if (itmd->mode == ITMD_OUTER_CLIENT) itm_outer_cnt--;
	else if (itmd->mode == ITMD_INNER_CLIENT) itm_inner_cnt--;
	else {
		itm_log(ITML_ERROR, "[ERROR] error to close a listen socket");
		return -1;
	}
	mode = itmd->mode;
	itmd->mode = -1;

	if (itmd->initok) apr_poll_del(itm_polld, itmd->fd);
	if (itmd->rstream.pool) ims_destroy(&itmd->rstream);
	if (itmd->wstream.pool) ims_destroy(&itmd->wstream);
	if (itmd->lstream.pool) ims_destroy(&itmd->lstream);
	if (itmd->timeid >= 0) {
		idt_remove((mode == ITMD_OUTER_CLIENT)? &itm_timeu : &itm_timec, itmd->timeid);
		itmd->timeid = -1;
	}

	if (itmd->initok) {
		if (mode == ITMD_OUTER_CLIENT) {
			// 从等待队列中移出
			if (itmd->wnode.queue) 
				iv_remove((struct IVQUEUE*)itmd->wnode.queue, &itmd->wnode);
			// TODO 增加通知channel的代码
			channel = itm_rchannel(itmd->channel);
			if (channel) {	// 发送离开数据
				channel->ccnum--;
				itm_param_set(0, itm_headlen + 4, ITMT_LEAVE, itmd->hid, itmd->tag);
				itm_write_dword(itm_data + itm_headlen, (apr_uint32)code);
				itm_send(channel, itm_data, itm_headlen + 4);
			}
			if (itm_booklen[0] > 0) {
				long i;
				itm_param_set(0, itm_headlen + 4, ITMT_LEAVE, itmd->hid, itmd->tag);
				itm_write_dword(itm_data + itm_headlen, (apr_uint32)code);
				for (i = 0; i < itm_booklen[0]; i++) {
					long chid = itm_book[0][i];
					channel = itm_rchannel(chid);
					if (channel != NULL && chid != itmd->channel) {
						itm_send(channel, itm_data, itm_headlen + 4);
					}
				}
			}
			itm_log(ITML_INFO, "closed user hid=%XH channel=%d drop=%d code=%d from %s", 
				itmd->hid, itmd->channel, itmd->dropped, code, itm_epname(itmd));
		}	else {
			// 允许所有客户的读事件，将会导致客户断开
			while (itmd->waitq.nodecnt) itm_permitr(itmd);
			// TODO 撤销该频道
			if (itmd->channel >= 0) {
				itm_wchannel(itmd->channel, NULL);
				itm_book_reset(itmd->channel);
			}
			// TODO 通知0号channel有channel断开
			channel = itm_rchannel(0);
			if (channel) {
				itm_param_set(0, itm_headlen + 4, ITMT_CHSTOP, itmd->channel, itmd->tag);
				itm_write_dword(itm_data + itm_headlen, (apr_uint32)code);
				itm_send(channel, itm_data, itm_headlen + 4);
			}
			itm_log(ITML_INFO, "closed channel hid=%XH channel=%d code=%d from %s", 
				itmd->hid, itmd->channel, code, itm_epname(itmd));
		}
	}
	apr_shutdown(itmd->fd, 2);
	apr_close(itmd->fd);
	imp_delnode(&itm_fds, itmd->node);
	itmd->hid = -1;
	itmd->node = -1;
	itmd->mode = -1;

	return 0;
}

//---------------------------------------------------------------------
// 处理发送事件
//---------------------------------------------------------------------
int itm_event_send(struct ITMD *itmd)
{
	long size = 0;
	
	switch (itmd->mode)
	{
	case ITMD_OUTER_CLIENT:
		size = itm_trysend(itmd);
		if (size < 0) {
			itm_log(ITML_INFO, "closing connection for remote lost: %d", 
				(itm_error == IEAGAIN)? 0 : itm_error);
			itm_event_close(itmd, itm_error);
			break;
		}
		if (itmd->wstream.size == 0) {
			if ((itmd->mask & APOLL_OUT) != 0) 
				itm_mask(itmd, 0, ITM_WRITE);
		}	else {
			if ((itmd->mask & APOLL_OUT) == 0)
				itm_mask(itmd, ITM_WRITE, 0);
		}
		if (size > 0 && itm_utiming == 0) idt_active(&itm_timeu, itmd->timeid);
		break;

	case ITMD_INNER_CLIENT:
		size = itm_trysend(itmd);
		if (size < 0) {
			itm_log(ITML_INFO, "closing connection for remote lost: %d", 
				(itm_error == IEAGAIN)? 0 : itm_error);
			itm_event_close(itmd, itm_error);
			break;
		}
		if (itmd->wstream.size == 0) {
			if ((itmd->mask & APOLL_OUT) != 0) 
				itm_mask(itmd, 0, ITM_WRITE);
		}	else {
			if ((itmd->mask & APOLL_OUT) == 0)
				itm_mask(itmd, ITM_WRITE, 0);
		}
		if (size > 0) {
			idt_active(&itm_timec, itmd->timeid);
			if (itmd->wstream.size < itm_inner_blimit && itmd->waitq.nodecnt)
				itm_permitr(itmd);
		}
		break;

	case ITMD_DGRAM_HOST4:
		itm_trysendto(AF_INET);
		if (itm_dgramdat4.size == 0) itm_mask(&itmd_dgram4, 0, ITM_WRITE);
		break;
	
	case ITMD_DGRAM_HOST6:
	#ifdef AF_INET6
		itm_trysendto(AF_INET6);
	#endif
		if (itm_dgramdat6.size == 0) itm_mask(&itmd_dgram6, 0, ITM_WRITE);
		break;
	}

	return 0;
}

//---------------------------------------------------------------------
// 处理接收事件
//---------------------------------------------------------------------
int itm_event_recv(struct ITMD *itmd) 
{
	struct ITMD *channel;
	long retval = 0;
	long length = 0;
	
	if (itmd->mode == ITMD_OUTER_CLIENT) {
		// 如果已经再等待队列中就静止读事件并退出
		if (itmd->wnode.queue) { 
			itm_mask(itmd, 0, ITM_READ);
			return 0;
		}
		channel = itm_rchannel(itmd->channel);
		if (channel == NULL) {
			itm_log(ITML_WARNING, 
				"[WARNING] data refused: channel %d does not exist", 
				itmd->channel);
			itm_event_close(itmd, 2302);
			return 0;
		}
		// 频道写缓存已经满，则静止读操作，放入等待队列
		if (channel->wstream.size > itm_inner_blimit) {
			itm_mask(itmd, 0, ITM_READ);
			if (itmd->wnode.queue == NULL) 
				iv_tailadd(&channel->waitq, &itmd->wnode);
			if (itm_logmask & ITML_DATA) {
				itm_log(ITML_DATA, 
					"channel buffer full disable read hid=%XH channel=%d", 
					itmd->hid, itmd->channel);
			}
			return 0;
		}
		// 如果其还在某个channel的等待队列里面的话，也暂停读消息
		if (itmd->wnode.queue != NULL) {
			itm_mask(itmd, 0, ITM_READ);
			return 0;
		}
	}

	// 从网络接受所有数据
	retval = itm_tryrecv(itmd);
	if (retval == 0) return 0;

	if (retval < 0) {
		itm_log(ITML_INFO, "remote socket disconnected: %d", 
			(itm_error == IEAGAIN)? 0 : itm_error);
		itm_event_close(itmd, itm_error);
		return 0;
	}

	// 激活当前的超时设置
	idt_active((itmd->mode == ITMD_OUTER_CLIENT)? &itm_timeu : &itm_timec, itmd->timeid);

	// 跳过 HTTP头部模式
	if (itm_httpskip) {
		if (itmd->mode == ITMD_OUTER_CLIENT && itmd->skipped == 0) {
			struct IMSTREAM *stream = &(itmd->rstream);
			while (stream->size > 0 && itmd->skipped == 0) {
				itmd->history1 = itmd->history2;
				itmd->history2 = itmd->history3;
				itmd->history3 = itmd->history4;
				ims_read(stream, &(itmd->history4), 1);
				if (itmd->history1 == '\r' && itmd->history2 == '\n' &&
					itmd->history3 == '\r' && itmd->history4 == '\n') {
					itmd->skipped = 1;
				}
			}
		}
	}	

	// 处理接收下来的所有包
	for (;;) {
		// 判断是否有一个完整的包
		length = itm_dataok(itmd);
		if (length == 0) break;		// 没有就返回
		if (length < 0) { 
			const char *msg = (length == -1)? "error" : "invalid";
			itm_log(ITML_INFO, "data size %s hid=%XH channel=%d from %s", 
				msg, itmd->hid, itmd->channel, itm_epname(itmd));
			itm_event_close(itmd, 2001); 
			break; 
		}
		if (length > itm_datamax) {
			itm_log(ITML_INFO, "data length is too long hid=%HX channel=%d from %s",
				itmd->hid, itmd->channel, itm_epname(itmd));
			itm_event_close(itmd, 2002);
			break;
		}
		if (itmd->mode == ITMD_INNER_CLIENT) {
			ims_read(&itmd->rstream, itm_data, length);
			retval = itm_data_inner(itmd);
		}
		else {
			if (itm_headmod != ITMH_RAWDATA) {		// 带长度数据
				ims_read(&itmd->rstream, itm_data, length);
			}	else {								// 不带长度数据
				ims_read(&itmd->rstream, itm_data + itm_hdrsize, length);
				itm_size_set(itm_data, length + itm_hdrsize);
			}
			retval = itm_data_outer(itmd);
		}
		if (retval < 0) { 
			itm_event_close(itmd, 2003); 
			break; 
		}
	}

	return 0;
}

//---------------------------------------------------------------------
// 收取数据报
//---------------------------------------------------------------------
int itm_event_dgram(int af)
{
	struct sockaddr_in remote4;
#ifdef AF_INET6
	struct sockaddr_in6 remote6;
#endif
	struct sockaddr *remote;
	struct ITMHUDP head;
	long size;

	for (size = 1; size > 0; ) {
		if (af == AF_INET) {
			remote = (struct sockaddr*)&remote4;
			size = apr_recvfrom(itm_dgram_sock4, itm_zdata, ITM_BUFSIZE, 0, remote, NULL);
		}	else {
		#ifdef AF_INET6
			int addrlen = sizeof(remote6);
			remote = (struct sockaddr*)&remote6;
			size = apr_recvfrom(itm_dgram_sock6, itm_zdata, ITM_BUFSIZE, 0, remote, &addrlen);
		#else
			size = 0;
		#endif
		}
		if (size >= 16) {
			apr_uint32 hid, session;
			idecode32u_lsb(itm_zdata +  0, &head.order);
			idecode32u_lsb(itm_zdata +  4, &head.index);
			idecode32u_lsb(itm_zdata +  8, &hid);
			idecode32u_lsb(itm_zdata + 12, &session);
			head.hid = (apr_int32)hid;
			head.session = (apr_int32)session;
			if (head.order < 0x80000000 || (head.order >> 24) == 0xff) {
				itm_dgram_data(af, remote, &head, itm_zdata + 16, size - 16);
			}	else {
				itm_dgram_cmd(af, remote, &head, itm_zdata + 16, size - 16);
			}
		}	else
		if (size > 0 && (itm_logmask & ITML_LOST)) {
			itm_log(ITML_LOST, "dgram data format error from ", itm_ntop(af, remote));
		}
	}
	return 0;
}


//---------------------------------------------------------------------
// 数据加密和IP验证模块函数指针
//---------------------------------------------------------------------
int (*itm_encrypt)(void *output, const void *input, int length, int fd, int mode);	
int (*itm_validate)(const void *sockaddr);

//---------------------------------------------------------------------
// 收到一个完整的外部包
//---------------------------------------------------------------------
int itm_data_outer(struct ITMD *itmd)
{
	long length;
	struct ITMD *channel;
	int category;

	channel = itm_rchannel(itmd->channel);
	if (channel == NULL) {
		itm_log(ITML_WARNING, 
			"[WARNING] data refused: channel %d does not exist", itmd->channel);
		return -1;
	}

	length = itm_size_get(itm_data);
	category = -1;

	if (itm_headmsk) {
		category = itm_cate_get(itm_data);
	}

	// 如果没有加密则直接转发
	itm_param_set(length, (length + itm_headlen - itm_hdrsize), ITMT_DATA, itmd->hid, itmd->tag);

	// 没有设置过分类的消息才发送到原频道
	if (category <= 0) {
		itm_send(channel, itm_data + length, itm_headlen);
		itm_send(channel, itm_data + itm_hdrsize, length - itm_hdrsize);
		if (itm_logmask & ITML_DATA) {
			long logsize = length;
			if (itm_headmod == ITMH_RAWDATA) logsize -= itm_hdrsize;
			itm_log(ITML_DATA, "recv %ld bytes data from hid=%XH channel=%d %s", 
				logsize, itmd->hid, itmd->channel, itm_epname(itmd));
		}
	}

	// 如果频道缓存允许则继续允许缓存中其他的读事件
	if (channel->wstream.size < itm_inner_blimit) itm_permitr(channel);

	// 增加收包计数器
	ITMDINCD(itmd->cnt_tcpr);

	// 增加全局统计
	itm_stat_recv++;

	// 发送到订阅了该消息的频道
	if (itm_headmsk && category > 0 && category < 255) {
		short *book = itm_book[category];
		int booklen = itm_booklen[category];
		int chid, i;
		for (i = 0; i < booklen; i++) {
			chid = book[i];
			channel = itm_rchannel(chid);
			if (channel) {
				// 频道写缓存已经满，则静止读操作，放入等待队列
				if (channel->wstream.size > itm_inner_blimit) {
					itm_mask(itmd, 0, ITM_READ);
					if (itmd->wnode.queue == NULL) 
						iv_tailadd(&channel->waitq, &itmd->wnode);
					if (itm_logmask & ITML_DATA) {
						itm_log(ITML_DATA, 
							"channel buffer full disable read hid=%XH channel=%d dstch=%d", 
							itmd->hid, itmd->channel, channel->channel);
					}
					return 0;
				}
				itm_send(channel, itm_data + length, itm_headlen);
				itm_send(channel, itm_data + itm_hdrsize, length - itm_hdrsize);
				// 如果频道缓存允许则继续允许缓存中其他的读事件
				if (channel->wstream.size < itm_inner_blimit) itm_permitr(channel);
			}
		}
	}

	return 0;
}

//---------------------------------------------------------------------
// 收到一个完整的内部包
//---------------------------------------------------------------------
int itm_data_inner(struct ITMD *itmd)
{
	long event, length = 0, retval;
	long wparam = 0, lparam = 0;
	short cmd = 0;

	// 处理频道登陆
	if (itmd->channel < 0) {
		retval = itm_on_logon(itmd);
		return retval;
	}

	// 读取基本信息
	itm_param_get(itm_data, &length, &cmd, &wparam, &lparam);

	event = cmd;
	retval = 0;

	// 处理频道发送的各种信令
	switch (event)
	{
	case ITMC_DATA:		// 处理发送数据部分
		retval = itm_on_data(itmd, wparam, lparam, length);
		break;

	case ITMC_CLOSE:	// 处理断开连接部分
		retval = itm_on_close(itmd, wparam, lparam, length);
		break;

	case ITMC_TAG:		// 处理设置TAG
		retval = itm_on_tag(itmd, wparam, lparam, length);
		break;

	case ITMC_CHANNEL:	// 处理组间通信
		retval = itm_on_channel(itmd, wparam, lparam, length);
		break;

	case ITMC_MOVEC:	// 处理移动ITMD
		retval = itm_on_movec(itmd, wparam, lparam, length);
		break;
	
	case ITMC_UNRDAT:	// 发送数据报
		retval = itm_on_dgram(itmd, wparam, lparam, length);
		break;
	
	case ITMC_IOCTL:	// 连接控制指令
		retval = itm_on_ioctl(itmd, wparam, lparam, length);
		break;

	case ITMC_SYSCD:	// 处理系统信息
		retval = itm_on_syscd(itmd, wparam, lparam, length);
		break;
	
	case ITMC_BROADCAST:	// 处理数据广播
		retval = itm_on_broadcast(itmd, wparam, lparam, length);
		break;

	case ITMC_NOOP:		// 处理空消息
		itm_param_set(0, length, ITMT_NOOP, wparam, lparam);
		itm_send(itmd, itm_data, length);
		break;

	default:			// 处理默认消息
		itm_log(ITML_WARNING, "[WARNING] channel %d send unknow command [%d]", 
			itmd->channel, event);
		break;
	}

	return retval;
}

//---------------------------------------------------------------------
// 频道命令：处理频道登陆
//---------------------------------------------------------------------
int itm_on_logon(struct ITMD *itmd)
{
	struct ITMD *channel;
	long length, c, i;

	length = itm_size_get(itm_data);

	if (length != itm_hdrsize + 2) {
		itm_log(ITML_WARNING, 
			"[WARNING] channel cannot start, channel id is unknow");
		return -1;
	}

	c = itm_read_word(itm_data + itm_hdrsize);

	if (c == 0xffff) {
		if (itm_dhcp_index > itm_dhcp_high) itm_dhcp_index = itm_dhcp_base;
		if (itm_dhcp_index < itm_dhcp_base) itm_dhcp_index = itm_dhcp_base;
		for (i = itm_dhcp_base; i < itm_dhcp_high; i++) {
			int index = itm_dhcp_index++;
			if (itm_dhcp_index >= itm_dhcp_high) 
				itm_dhcp_index = itm_dhcp_base;
			if (itm_rchannel(index) == NULL) {
				c = index;
				break;
			}
		}
	}

	if (itm_rchannel(c)) {
		itm_log(ITML_WARNING, 
			"[WARNING] closing connection because channel %d is already used: ", c);
		return -2;
	}

	if (itm_wchannel(c, itmd)) {
		itm_log(ITML_WARNING, "[WARNING] can not set channel to channel %d", c);
		return -3;
	}
	itmd->channel = c;

	itm_log(ITML_INFO, "channel %d started from connection hid=%XH %s", 
		c, itmd->hid, itm_epname(itmd));

	channel = itm_rchannel(0);
	if (channel && c != 0) {	// 如果其他频道则通知通道 0
		itm_param_set(0, itm_headlen, ITMT_CHNEW, c, itmd->hid);
		itm_send(channel, itm_data, itm_headlen);
	}
	if (c == 0) {				// 如果是频道 0则复位频道消息
		itm_interval = 0;
		itm_notice_slap = 0;
		itm_notice_cycle = -1;
		itm_notice_count = 0;
	}

	return 0;
}

//---------------------------------------------------------------------
// 频道命令：发送数据到用户
//---------------------------------------------------------------------
int itm_on_data(struct ITMD *itmd, long wparam, long lparam, long length)
{
	struct ITMD *to = itm_hid_itmd(wparam);
	long dlength = length - itm_headlen;
	int cansend = 1;
	char *ptr;

	if (to == NULL) { 
		if (itm_logmask & ITML_WARNING) {
			itm_log(ITML_WARNING, 
				"[WARNING] cannot send data to hid %XH from channel %d for hid error", 
				wparam, itmd->channel);
		}
		return 0;
	}
	if (to->channel != itmd->channel && itmd->channel != 0 && itm_headmod < ITMH_DWORDMASK) { 
		if (itm_logmask & ITML_WARNING) {
			itm_log(ITML_WARNING, 
				"[WARNING] cannot send data to hid %XH from channel %d", 
				wparam, itmd->channel);
		}
		return 0;
	}
	if (to->mode != ITMD_OUTER_CLIENT) {
		if (itm_logmask & ITML_WARNING) {
			itm_log(ITML_WARNING,
				"[WARNING] cannot send data to hid %XH from channel %d for mode error",
				wparam, itmd->channel);
		}
		return 0;
	}

	// 向具体的连接发送数据
	ptr = itm_data + itm_headlen - itm_hdrsize;
	itm_size_set(ptr, dlength + itm_hdrsize);

	// 如果要缓存判断大小再决定发送的话
	if (lparam & 0x40000000) {
		long limit = lparam & 0x3fffffff;
		if (to->wstream.size >= limit) cansend = 0;		// 设置禁止发送
	}

	// 如果可以发送：不需要判断缓存大小或者判断成功
	if (cansend) {
		long sendlen;
		char *data;

		// 判断是否是 RAWDATA, LINESPLIT模式：
		if (itm_headmod < ITMH_RAWDATA) {
			data = ptr;
			sendlen = dlength + itm_hdrsize;
		}	else {
			data = ptr + itm_hdrsize;
			sendlen = dlength;
		}

		#ifndef IDISABLE_RC4
		// 如果有加密
		if (to->rc4_send_x >= 0 && to->rc4_send_y >= 0) {
			itm_rc4_crypt(to->rc4_send_box, &to->rc4_send_x, &to->rc4_send_y,
				(const unsigned char*)data, (unsigned char*)itm_crypt, sendlen);
			data = itm_crypt;
		}
		#endif

		// 送入发送缓存
		itm_send(to, data, sendlen);

		if (itm_logmask & ITML_DATA) {
			long logsize = dlength + itm_hdrsize;
			if (itm_headmod == ITMH_RAWDATA) logsize = dlength;
			itm_log(ITML_DATA, "channel %d send %ld bytes data to hid=%XH %s", 
				itmd->channel, logsize, to->hid, itm_epname(to));
		}

		itm_bcheck(to);
		itm_stat_send++;
	}	else {
		if (itm_logmask & ITML_DATA) {
			itm_log(ITML_DATA, "channel %d discard sending data to hid=%XH %s",
				itmd->channel, to->hid, itm_epname(to));
		}
		itm_stat_discard++;
	}

	return 0;
}


//---------------------------------------------------------------------
// 频道命令：关闭用户连接
//---------------------------------------------------------------------
int itm_on_close(struct ITMD *itmd, long wparam, long lparam, long length)
{
	struct ITMD *to = itm_hid_itmd(wparam);

	if (to == NULL) { 
		itm_log(ITML_WARNING, 
			"[WARNING] cannot close hid %XH from channel %d for hid error", 
			wparam, itmd->channel);
		return 0;
	}
	if (to->channel != itmd->channel && itmd->channel != 0 && itm_headmod < ITMH_DWORDMASK) {
		itm_log(ITML_WARNING, 
			"[WARNING] cannot close hid %XH from channel %d for hid not in channel", 
			wparam, itmd->channel);
		return 0;
	}

	// 最后尝试发送一次数据
	if (to->wstream.size > 0) {
		itm_trysend(to);
	}

	itm_log(ITML_INFO, "channel %d closing user hid=%XH %s: %d", 
		itmd->channel, to->hid, itm_epname(to), lparam);

	itm_event_close(to, lparam);

	wparam = wparam;
	lparam = lparam;
	length = length;

	return 0;
}

//---------------------------------------------------------------------
// 频道命令：设置标签
//---------------------------------------------------------------------
int itm_on_tag(struct ITMD *itmd, long wparam, long lparam, long length)
{
	struct ITMD *to = itm_hid_itmd(wparam);
	if (to == NULL) { 
		itm_log(ITML_WARNING, 
			"[WARNING] cannot set tag to hid %XH from channel %d for hid error", 
			wparam, itmd->channel);
		return 0; 
	}
	if (to->channel != itmd->channel && itmd->channel != 0 && itm_headmod < ITMH_DWORDMASK) { 
		itm_log(ITML_WARNING, 
			"[WARNING] cannot set tag to hid %XH from channel %d form hid not in channel", 
			wparam, itmd->channel);
		return 0; 
	}
	to->tag = lparam;
	itm_log(ITML_DATA, "channel %d set tag to %d for hid=%XH %s: ", 
		itmd->channel, lparam, wparam, itm_epname(to));

	wparam = wparam;
	lparam = lparam;
	length = length;

	return 0;
}


//---------------------------------------------------------------------
// 频道命令：频道间通信
//---------------------------------------------------------------------
int itm_on_channel(struct ITMD *itmd, long wparam, long lparam, long length)
{
	struct ITMD *channel = itm_rchannel(wparam);
	struct IDTIMEN *tmnode;
	long c;

	if (channel == NULL && wparam >= 0) { 
		itm_log(ITML_WARNING, 
			"[WARNING] channel %d cannot send data to channel %d for channel error", 
			itmd->channel, wparam);
		return 0; 
	}

	itm_write_dword(itm_data + itm_hdrsize + 2, (apr_uint32)itmd->channel);
	itm_write_dword(itm_data + itm_hdrsize + 6, (apr_uint32)itmd->tag);

	// 判断是单发还是群发
	if (wparam >= 0) {	// 如果是单发
		if (itm_logmask & ITML_CHANNEL) {
			itm_log(ITML_CHANNEL, 
				"channel %d send %d bytes data to channel %d", 
				itmd->channel, length, wparam);
		}
		itm_send(channel, itm_data, length);
		itm_bcheck(channel);
	}	else {			// 如果是群发
		for (tmnode = itm_timec.tail, c = 0; tmnode; tmnode = tmnode->next) {
			channel = itm_hid_itmd(tmnode->data);
			if (channel != NULL) {
				if (channel->channel != itmd->channel) {
					itm_send(channel, itm_data, length);
					itm_bcheck(channel);
					c++;
				}
			}	else {
				itm_log(ITML_ERROR, 
					"[ERROR] list channel error in time queue hid=%XH", 
					tmnode->data);
			}
		}
		if (itm_logmask & ITML_CHANNEL) {
			itm_log(ITML_CHANNEL, "channel %d send %d bytes data to %d channels", 
				itmd->channel, length, c);
		}
	}

	lparam = lparam;

	return 0;
}

//---------------------------------------------------------------------
// 频道命令：移动用户连接到某频道
//---------------------------------------------------------------------
int itm_on_movec(struct ITMD *itmd, long wparam, long lparam, long length)
{
	struct ITMD *channel = itm_rchannel(wparam);
	struct ITMD *to = itm_hid_itmd(lparam);
	struct ITMD *ochannel;

	if (to == NULL) { 
		itm_log(ITML_WARNING, 
			"[WARNING] channel %d can not move user connection to channel %d for hid %XH error", 
			itmd->channel, wparam, lparam);
		return 0;
	}
	if (channel == NULL) { 
		itm_log(ITML_WARNING, 
			"[WARNING] channel %d can not move user connection to channel %d for channel %d does not exist", 
			itmd->channel, wparam, wparam);
		itm_event_close(to, 0);
		return 0;
	}
	if (to->channel != itmd->channel && itmd->channel != 0) {
		itm_log(ITML_WARNING, 
			"[WARNING] channel %d can not move user connection to channel %d for hid %XH does not belong to this channel", 
			itmd->channel, wparam, lparam);
		return 0; 
	}
	if (to->mode != ITMD_OUTER_CLIENT) {
		itm_log(ITML_WARNING, 
			"[WARNING] can not move user connection for hid=%XH is not a user connection", to->hid);
		return 0;
	}

	// 如果存在于某等待队列中
	if (to->wnode.queue) {	
		iv_remove((struct IVQUEUE*)to->wnode.queue, &to->wnode);
		itm_mask(to, ITM_READ, 0);
	}

	ochannel = itm_rchannel(to->channel);
	if (ochannel) ochannel->ccnum--;
	to->channel = wparam;
	channel->ccnum++;

	if (itm_logmask & ITML_DATA) {
		itm_log(ITML_DATA, 
			"channel %d move user connection hid=%XH to channel %d", 
			itmd->channel, lparam, wparam);
	}
	itm_param_set(0, length, ITMT_NEW, to->hid, to->tag);
	itm_send(channel, itm_data, length);

	return 0;
}


//---------------------------------------------------------------------
// 频道命令：发送数据报
//---------------------------------------------------------------------
int itm_on_dgram(struct ITMD *itmd, long wparam, long lparam, long length)
{
	struct ITMD *to = itm_hid_itmd(wparam);
	long dlength = length - itm_headlen;

	if (to == NULL) { 
		if (itm_logmask & ITML_WARNING) {
			itm_log(ITML_WARNING, 
				"[WARNING] cannot send dgram to hid %XH from channel %d for hid error", 
				wparam, itmd->channel);
		}
		return 0;
	}
	if (to->channel != itmd->channel && itmd->channel != 0) {
		if (itm_headmod < ITMH_DWORDMASK && (itm_udpmask & (ITMU_MDUDP | ITMU_MDTCP)) != 0) { 
			if (itm_logmask & ITML_WARNING) {
				itm_log(ITML_WARNING, 
					"[WARNING] cannot send dgram to hid %XH from channel %d", 
					wparam, itmd->channel);
			}
			return 0;
		}
	}
	if ((itm_udpmask & ITMU_MWORK) == 0) {
		if (itm_logmask & ITML_WARNING) {
			itm_log(ITML_WARNING,
				"[WARNING] cannot send dgram to hid %XH from channel %d for udp disable",
				wparam, itmd->channel);
		}
		return 0;
	}
	if (to->touched == 0) {
		if (itm_logmask & ITML_WARNING) {
			itm_log(ITML_WARNING, 
				"[WARNING] cannot send dgram to hid %XH from channel %d for not touched", 
				wparam, itmd->channel);
		}
		return 0;
	}

	// 向具体的连接发送数据
	if (itm_logmask & ITML_DATA) {
		itm_log(ITML_DATA, "channel %d send %d bytes dgram to hid=%XH %s", 
			itmd->channel, dlength + 2, to->hid, itm_epname(to));
	}

	itm_sendto(to, itm_data + itm_headlen, dlength);
	lparam = lparam;

	return 0;
}

//---------------------------------------------------------------------
// 频道命令：系统控制消息
//---------------------------------------------------------------------
int itm_on_syscd(struct ITMD *itmd, long wparam, long lparam, long length)
{
	struct ITMD *channel, *ochannel;
	struct IDTIMEN *tmnode;
	int retval = 0, c;
	char *lptr;

	switch (wparam)
	{
	case ITMS_CONNC:
		itm_param_set(0, itm_headlen + 8, ITMT_SYSCD, ITMS_CONNC, 0);
		itm_write_dword(itm_data + itm_headlen + 0, (apr_uint32)itm_outer_cnt);
		itm_write_dword(itm_data + itm_headlen + 4, (apr_uint32)itm_inner_cnt);
		itm_send(itmd, itm_data, itm_headlen + 8);
		itm_log(ITML_CHANNEL, 
			"system info: %d user connection(s) and %d channel connection(s)", 
			itm_outer_cnt, itm_inner_cnt);
		break;

	case ITMS_LOGLV:
		itm_logmask = lparam;
		itm_log(ITML_INFO, "system info: set log mask to %XH", itm_logmask);
		break;

	case ITMS_LISTC:
		lparam = itm_timec.wtime + itm_timec.timeout;
		lparam = (lparam < 0)? 0x7fffffff : lparam;
		lptr = itm_data + itm_headlen;
		for (tmnode = itm_timec.tail, c = 0; tmnode; tmnode = tmnode->next) {
			channel = itm_hid_itmd(tmnode->data);
			if (channel) {
				wparam = lparam - tmnode->time;
				wparam = (wparam < 0)? 0 : ((wparam >= 0x10000)? 0xffff : wparam);
				itm_write_dword(lptr, (apr_uint32)channel->channel); lptr += 4;
				itm_write_dword(lptr, (apr_uint32)channel->hid); lptr += 4;
				itm_write_dword(lptr, (apr_uint32)channel->tag); lptr += 4;
				itm_write_dword(lptr, (apr_uint32)((wparam & 0xffff) | (channel->ccnum << 16))); lptr += 4;
				c++;
			}	else {
				itm_log(ITML_ERROR, "[ERROR] list channel error in time queue hid=%XH", 
					tmnode->data);
			}
		}
		itm_param_set(0, (lptr - itm_data), ITMT_SYSCD, ITMS_LISTC, c);
		itm_send(itmd, itm_data, lptr - itm_data);
		itm_log(ITML_CHANNEL, "system info: list info for %d channel(s)", c);
		break;

	case ITMS_RTIME:
		itm_param_set(0, itm_headlen, ITMT_SYSCD, ITMS_RTIME, itm_wtime);
		itm_send(itmd, itm_data, itm_headlen);
		itm_log(ITML_INFO, "system info: service is running for %d second(s)", itm_wtime);
		break;

	case ITMS_TMVER:
		itm_param_set(0, itm_headlen, ITMT_SYSCD, ITMS_TMVER, ITMV_VERSION);
		itm_send(itmd, itm_data, itm_headlen);
		itm_log(ITML_INFO, "system info: transmod version is %x", ITMV_VERSION);
		break;

	case ITMS_REHID:
		ochannel = itm_rchannel(lparam);
		wparam = (ochannel)? ochannel->hid : -1;
		itm_param_set(0, itm_headlen, ITMT_SYSCD, ITMS_REHID, wparam);
		itm_send(itmd, itm_data, itm_headlen);
		itm_log(ITML_INFO, "system info: hid of channel %d is %XH", lparam, wparam);
		break;

	case ITMS_TIMER:
		if (itmd->channel != 0) {
			itm_log(ITML_WARNING, "system info: channel %d cannot access timer", itmd->channel);
			break;
		}
		if (lparam > 0) {
			itm_notice_count = 0;
			itm_notice_cycle = lparam;
			itm_notice_slap = itm_time_current;
			itm_notice_saved = itm_time_current;
			itm_log(ITML_INFO, "system info: channel 0 set timer on %dms", lparam);
		}	else {
			itm_notice_slap = 0;
			itm_notice_cycle = -1;
			itm_notice_count = 0;
			itm_log(ITML_INFO, "system info: channel 0 set timer off");
		}
		break;
	
	case ITMS_INTERVAL:
		if (itmd->channel != 0) {
			itm_log(ITML_WARNING, "system info: channel %d cannot change interval mode", itmd->channel);
			break;
		}
		itm_interval = lparam;
		itm_log(ITML_INFO, "system info: channel 0 set interval to %d", lparam);
		break;
	
	case ITMS_FASTMODE:
		if (itmd->channel != 0) {
			itm_log(ITML_WARNING, "system info: channel %d cannot set fast mode", itmd->channel);
			break;
		}
		itm_fastmode = lparam;
		itm_log(ITML_INFO, "system info: channel 0 set fast mode to %d", lparam);
		break;

	case ITMS_CHID:
		itm_param_set(0, itm_headlen, ITMT_SYSCD, ITMS_CHID, itmd->channel);
		itm_send(itmd, itm_data, itm_headlen);
		itm_log(ITML_INFO, "system info: channel %d get id", itmd->channel);
		break;
	
	case ITMS_BOOKADD:
		itm_book_add(lparam, itmd->channel);
		itm_log(ITML_INFO, "system info: channel %d subscribe add %d", itmd->channel, lparam);
		break;

	case ITMS_BOOKDEL:
		itm_book_del(lparam, itmd->channel);
		itm_log(ITML_INFO, "system info: channel %d subscribe del %d", itmd->channel, lparam);
		break;

	case ITMS_BOOKRST:
		itm_book_reset(itmd->channel);
		itm_log(ITML_INFO, "system info: channel %d subscribe reset", itmd->channel);
		break;

	case ITMS_QUITD:
		itm_log(ITML_INFO, "system info: channel %d asks to quit", itmd->channel);
		retval = -10;
		break;
	
	case ITMS_DISABLE:
		{
			struct ITMD *target = itm_hid_itmd(lparam);
			if (target == NULL) {
				itm_log(ITML_WARNING, "[WARNING] can not disable user hid=%XH channel=%d",
					lparam, itmd->channel);
				break;
			}
			if (target->disable == 0) {
				itm_mask(target, 0, ITM_READ);
				target->disable = 1;
			}
		}
		break;
	
	case ITMS_ENABLE:
		{
			struct ITMD *target = itm_hid_itmd(lparam);
			if (target == NULL) {
				itm_log(ITML_WARNING, "[WARNING] can not enable user hid=%XH channel=%d",
					lparam, itmd->channel);
				break;
			}
			if (target->disable != 0) {
				if (target->wnode.queue == NULL) {
					itm_mask(target, ITM_READ, 0);
				}
				target->disable = 0;
			}
		}
		break;

	case ITMS_SETDOC: {
			int size = (length >= itm_headlen)? (int)(length - itm_headlen) : 0;
			itm_version++;
			if (itm_document && size <= itm_datamax) {
				memcpy(itm_document, itm_data + itm_headlen, size);
				itm_document[size] = 0;
			}
			itm_docsize = size;
			itm_version++;
		}
		break;

	case ITMS_GETDOC: 
		itm_param_set(0, itm_headlen + itm_docsize, ITMT_SYSCD, ITMS_GETDOC, 0);
		if (itm_document) {
			memcpy(itm_data + itm_headlen, itm_document, itm_docsize);
		}
		itm_send(itmd, itm_data, itm_headlen + itm_docsize);
		break;

	case ITMS_MESSAGE: 
		itm_msg_put(1, itm_data + itm_headlen, (long)(length - itm_headlen));
		break;

	case ITMS_STATISTIC:
		{
			char text1[32];
			char text2[32];
			char text3[32];
			itm_lltoa(text1, itm_stat_send);
			itm_lltoa(text2, itm_stat_recv);
			itm_lltoa(text3, itm_stat_discard);
			itm_log(ITML_INFO, "system info: statistic send=%s recv=%s discard=%s",
				text1, text2, text3);
		}
		break;
	
	case ITMS_RC4SKEY:
	case ITMS_RC4RKEY:
		{
			#ifndef IDISABLE_RC4
			struct ITMD *target = itm_hid_itmd(lparam);
			unsigned char *key = (unsigned char*)itm_data + itm_headlen;
			int keylen = (length >= itm_headlen)? (int)(length - itm_headlen) : 0;
			if (target == NULL) {
				itm_log(ITML_WARNING, "[WARNING] can not set rc4 key to hid=%XH channel=%d",
					lparam, itmd->channel);
				break;
			}
			if (target->mode != ITMD_OUTER_CLIENT) {
				itm_log(ITML_ERROR, "[ERROR] channel %d cannot set rc4 key for hid=%HX",
					itmd->channel, lparam);
				break;
			}
			if (wparam == ITMS_RC4SKEY) {
				itm_rc4_init(target->rc4_send_box, &target->rc4_send_x, &target->rc4_send_y, key, keylen);
			}
			else {
				itm_rc4_init(target->rc4_recv_box, &target->rc4_recv_x, &target->rc4_recv_y, key, keylen);
			}
			itm_log(ITML_INFO, "system info: channel %d set %s crypt key for hid=%XH",
				itmd->channel, (wparam == ITMS_RC4SKEY)? "send" : "recv", lparam);
			#else
			itm_log(ITML_ERROR, "[ERROR] rc4 crypt is not supported");
			#endif
		}
		break;
	}

	return retval;
}


//---------------------------------------------------------------------
// 处理报文数据
//---------------------------------------------------------------------
int itm_dgram_data(int af, struct sockaddr *remote, struct ITMHUDP *head, void *data, long size)
{ 
	struct ITMD *itmd;
	struct ITMD *channel;
	int category = 0;

	itmd = itm_hid_itmd(head->hid);
	if (itmd == NULL) {
		itm_log(ITML_LOST, "[DROP] dgram session error from %s", itm_ntop(af, remote));
		return 0;
	}
	if (itmd->session != head->session) {
		itm_log(ITML_LOST, "[DROP] dgram session error for hid=%XH channel=%d",
			head->hid, itmd->channel);
		return 0;
	}
	channel = itm_rchannel(itmd->channel);
	if (channel == NULL) {
		itm_log(ITML_WARNING, 
			"[WARNING] dgram refused: channel %d does not exist", itmd->channel);
		return -1;
	}
	
	if ((head->order >> 24) == 0xff) {
		category = head->order & 0xffff;
		head->order = 0;
	}

	if (head->order < itmd->cnt_udpr && (itm_udpmask & ITMU_MDUDP)) {
		if (itm_logmask & ITML_LOST) {
			itm_log(ITML_LOST, "[DROP] dgram udp order error hid=%x channel=%x", 
				itmd->hid, itmd->channel);
		}
		itmd->dropped++;
		itm_dropped++;
		return -2;
	}

	itmd->cnt_udpr = (head->order > itmd->cnt_udpr)? head->order : itmd->cnt_udpr;
	
	if (head->index != itmd->cnt_tcpr && (itm_udpmask & ITMU_MDTCP)) {
		if (itm_logmask & ITML_LOST) {
			itm_log(ITML_LOST, "[DROP] dgram tcp index error hid=%x channel=%x", 
				itmd->hid, itmd->channel);
		}
		itmd->dropped++;
		itm_dropped++;
		return -3;
	}

	if (category < 0x100) {
		itm_param_set(0, (size + itm_headlen), ITMT_UNRDAT, itmd->hid, itmd->tag);
		itm_send(channel, itm_data, itm_headlen);
		itm_send(channel, data, size);
		if (itm_logmask & ITML_DATA) {
			itm_log(ITML_DATA, "recv %d bytes dgram from hid=%XH channel=%d %s", 
				size, itmd->hid, itmd->channel, itm_epname(itmd));
		}
	}	
	else if (category < 0x200 && itm_booklen[category] > 0) {
		long len, i;
		len = itm_booklen[category];
		itm_param_set(0, (size + itm_headlen), ITMT_UNRDAT, itmd->hid, itmd->tag);
		for (i = 0; i < len; i++) {
			long chid = itm_book[category][i];
			channel = (struct ITMD*)itm_rchannel(chid);
			if (channel != NULL && chid != itmd->channel) {
				itm_send(channel, itm_data, itm_headlen);
				itm_send(channel, data, size);
			}
		}
		if (itm_logmask & ITML_DATA) {
			itm_log(ITML_DATA, "recv %d bytes dgram from hid=%XH channel=%d (category=%d) %s", 
				size, itmd->hid, itmd->channel, category, itm_epname(itmd));
		}
	}

	return 0;
}

//---------------------------------------------------------------------
// 处理报文命令
//---------------------------------------------------------------------
int itm_dgram_cmd(int af, struct sockaddr *remote, struct ITMHUDP *head, void *data, long size)
{
	unsigned long sender, port;
	int cmd = head->order & 0x7fffffff;
	struct sockaddr_in *ep;
	struct ITMD *itmd;
	int addrlen = sizeof(struct sockaddr_in);

#ifdef AF_INET6
	addrlen = sizeof(struct sockaddr_in6);
#endif

	switch(cmd) 
	{
	case ITMU_TOUCH:
		itmd = itm_hid_itmd(head->hid);
		if (itmd == NULL) {
			itm_log(ITML_LOST, "[DROP] dgram touching failed to hid=%XH from %s",
				head->hid, itm_ntop(af, remote));
			break;
		}
		if (itmd->session != head->session) {
			itm_log(ITML_LOST, "[DROP] dgram touching error to hid=%XH from %s",
			head->hid, itm_ntop(af, remote));
			break;
		}
		if (af == AF_INET) {
			memcpy(&(itmd->dgramp4), remote, sizeof(struct sockaddr));
		#ifdef AF_INET6
			memset(&(itmd->dgramp6), 0, sizeof(itmd->dgramp6));
		#endif
		}	else {
			memset(&(itmd->dgramp4), 0, sizeof(itmd->dgramp4));
		#ifdef AF_INET6
			memcpy(&(itmd->dgramp6), remote, sizeof(struct sockaddr_in6));
		#endif
		}
		itmd->touched = 1;
		itm_sendudp(af, remote, head, data, size);
		itm_log(ITML_INFO, "dgram touched to hid=%XH from %s", head->hid, itm_ntop(af, remote));
		break;
	case ITMU_ECHO:
		itm_sendudp(af, remote, head, data, size);
		break;
	case ITMU_MIRROR:
		itm_sendudp(af, remote, head, remote, addrlen);
		break;
	case ITMU_DELIVER:
		if (af == AF_INET) {
			ep = (struct sockaddr_in*)remote;
			ep->sin_addr.s_addr = head->index;
			ep->sin_port = htons((unsigned short)(head->session));
			itm_sendudp(af, remote, NULL, data, size);
		}	else {
	#ifdef AF_INET6
			itm_sendudp(af, (struct sockaddr*)data, NULL, 
				(const char*)data + addrlen, size - addrlen);
	#endif
		}
		break;
	case ITMU_FORWARD:
		if (af == AF_INET) {
			ep = (struct sockaddr_in*)remote;
			sender = ep->sin_addr.s_addr;
			port = ntohs(ep->sin_port);
			ep->sin_addr.s_addr = head->index;
			ep->sin_port = htons((unsigned short)(head->session));
			head->index = sender;
			head->session = htons((unsigned short)(port));
			itm_sendudp(af, remote, head, data, size);
		}	else {
	#ifdef AF_INET6
			struct sockaddr_in6 target;
			memcpy(&target, data, addrlen);
			memcpy(data, remote, addrlen);
			itm_sendudp(af, (struct sockaddr*)&target, head, 
				(const char*)data, size);
	#endif
		}
		break;
	default:
		break;
	}

	return 0;
}

//---------------------------------------------------------------------
// 连接控制指令
//---------------------------------------------------------------------
int itm_on_ioctl(struct ITMD *itmd, long wparam, long lparam, long length)
{
	struct ITMD *to = itm_hid_itmd(wparam);
	long valued = (lparam >> 4);
	long retval;

	switch (lparam & 0x0f)
	{
	case ITMS_NODELAY:
		if (to == NULL) {
			itm_log(ITML_WARNING, "[WARNING] channel %d cannot set nodelay to hid=%XH",
				itmd->channel, wparam);
			break;
		}
		valued = (valued)? 1 : 0;
		retval = apr_setsockopt(to->fd, IPPROTO_TCP, TCP_NODELAY, (char*)&valued, sizeof(valued));
		if (itm_logmask & ITML_DATA) {
			itm_log(ITML_DATA, "ioctl: channel %d set nodelay %d to hid=%XH result=%d",
					itmd->channel, valued, wparam, retval);
		}
		break;

	case ITMS_NOPUSH:
		if (to == NULL) {
			itm_log(ITML_WARNING, "[WARNING] channel %d cannot set nopush to hid=%XH",
				itmd->channel, wparam);
			break;
		}
		if (valued) retval = apr_enable(to->fd, APR_NOPUSH);
		else retval = apr_disable(to->fd, APR_NOPUSH);
		if (itm_logmask & ITML_DATA) {
			itm_log(ITML_DATA, "ioctl: channel %d set nopush %d to hid=%XH result=%d",
					itmd->channel, valued, wparam, retval);
		}
		break;
	
	case ITMS_PRIORITY:
		if (to == NULL) {
			itm_log(ITML_WARNING, "[WARNING] channel %d cannot set nopush to hid=%XH",
				itmd->channel, wparam);
			break;
		}
		#if defined(SOL_SOCKET) && defined(SO_PRIORITY)
		retval = apr_setsockopt(to->fd, SOL_SOCKET, SO_PRIORITY, (char*)&valued, sizeof(valued));
		if (itm_logmask & ITML_DATA) {
			itm_log(ITML_DATA, "ioctl: channel %d set priority %d to hid=%XH result=%d",
					itmd->channel, valued, wparam, retval);
		}
		#else
		itm_log(ITML_WARNING, "[WARNING] channel %d does not support to set priority", itmd->channel); 
		#endif
		break;

	case ITMS_TOS:
		if (to == NULL) {
			itm_log(ITML_WARNING, "[WARNING] channel %d cannot set nopush to hid=%XH",
				itmd->channel, wparam);
			break;
		}
		#if defined(SOL_IP) && defined(IP_TOS)
		retval = apr_setsockopt(to->fd, SOL_IP, IP_TOS, (char*)&valued, sizeof(valued));
		if (itm_logmask & ITML_DATA) {
			itm_log(ITML_DATA, "ioctl: channel %d set TOS %d to hid=%XH result=%d",
					itmd->channel, valued, wparam, retval);
		}
		#else
		itm_log(ITML_WARNING, "[WARNING] channel %d does not support to set TOS", itmd->channel); 
		#endif
		break;
	}

	return 0;
}


//---------------------------------------------------------------------
// 处理数据广播
//---------------------------------------------------------------------
int itm_on_broadcast(struct ITMD *itmd, long wparam, long lparam, long length)
{
	apr_int64 stat_send = itm_stat_send;
	apr_int64 stat_discard = itm_stat_discard;
	long dlength = length - itm_headlen;
	long newsize = 0;
	long count = wparam;
	long success = 0;
	long discard = 0;
	char *ptr;
	long i;

	if (dlength < count * 4) {
		if (itm_logmask & ITML_WARNING) {
			itm_log(ITML_WARNING, 
				"[WARNING] broadcast data format error for channel %d",
				itmd->channel);
		}
		return 0;
	}

	newsize = length - count * 4;
	ptr = itm_data + newsize;

	for (i = 0; i < count; i++) {
		apr_uint32 hid;
		idecode32u_lsb(ptr, &hid);
		ptr += 4;
		itm_on_data(itmd, (long)hid, lparam, newsize);
	}

	success = (long)(itm_stat_send - stat_send);
	discard = (long)(itm_stat_discard - stat_discard);

	if (itm_logmask & ITML_CHANNEL) {
		itm_log(ITML_CHANNEL,
			"broadcast from channel %d: count=%ld success=%ld discard=%ld limit=%lx",
			itmd->channel, count, success, discard, lparam);
	}

	return 0;
}




