--- src/connection.c
+++ src/connection.c
@@ -223,6 +223,7 @@ build_cmsg(struct wl_buffer *buffer, char *data, int *clen)
 	size_t size;
 
 	size = buffer->head - buffer->tail;
+//	fprintf(stderr, "%s: size=%jd\n", __func__, size);
 	if (size > MAX_FDS_OUT * sizeof(int32_t))
 		size = MAX_FDS_OUT * sizeof(int32_t);
 
@@ -293,17 +294,25 @@ wl_connection_flush(struct wl_connection *connection)
 		msg.msg_namelen = 0;
 		msg.msg_iov = iov;
 		msg.msg_iovlen = count;
-		msg.msg_control = (clen > 0) ? cmsg : NULL;
+		if (clen == 0)
+			msg.msg_control = NULL;
+		else
+			msg.msg_control = cmsg;
 		msg.msg_controllen = clen;
 		msg.msg_flags = 0;
+//		fprintf(stderr, "%s: calling sendmsg, with:\n", __func__);
+//		fprintf(stderr, "msg.msg_iovlen=%jd, msg.msg_controllen=%jd\n",
+//		    msg.msg_iovlen, msg.msg_controllen);
 
 		do {
 			len = sendmsg(connection->fd, &msg,
 				      MSG_NOSIGNAL | MSG_DONTWAIT);
 		} while (len == -1 && errno == EINTR);
 
-		if (len == -1)
+		if (len == -1) {
+			warn("%s: sendmsg", __func__);
 			return -1;
+		}
 
 		close_fds(&connection->fds_out, MAX_FDS_OUT);
 
@@ -361,8 +370,23 @@ wl_connection_write(struct wl_connection *connection,
 	if (connection->out.head - connection->out.tail +
 	    count > ARRAY_LENGTH(connection->out.data)) {
 		connection->want_flush = 1;
-		if (wl_connection_flush(connection) < 0)
+		int count = 0;
+retry:
+		if (wl_connection_flush(connection) < 0) {
+			if (errno == EAGAIN) {
+				wl_log("%s: wl_connection_flush failed, "
+				    "retrying: %s\n", __func__,
+				    strerror(errno));
+				count++;
+				if (count > 10)
+					usleep(1);
+				goto retry;
+			} else {
+				wl_log("%s: wl_connection_flush failed: %s\n",
+				    __func__, strerror(errno));
+			}
 			return -1;
+		}
 	}
 
 	if (wl_buffer_put(&connection->out, data, count) < 0)
@@ -570,7 +594,7 @@ wl_closure_marshal(struct wl_object *sender, uint32_t opcode,
 			fd = args[i].h;
 			dup_fd = wl_os_dupfd_cloexec(fd, 0);
 			if (dup_fd < 0) {
-				wl_log("dup failed: %m");
+				wl_log("dup failed: %s\n", strerror(errno));
 				abort();
 			}
 			closure->args[i].h = dup_fd;
