CREATE TABLE `account` (
	`id` text PRIMARY KEY NOT NULL,
	`account_id` text NOT NULL,
	`provider_id` text NOT NULL,
	`user_id` text NOT NULL,
	`access_token` text,
	`refresh_token` text,
	`id_token` text,
	`access_token_expires_at` integer,
	`refresh_token_expires_at` integer,
	`scope` text,
	`password` text,
	`created_at` integer DEFAULT (cast(unixepoch('subsecond') * 1000 as integer)) NOT NULL,
	`updated_at` integer NOT NULL,
	FOREIGN KEY (`user_id`) REFERENCES `user`(`id`) ON UPDATE no action ON DELETE cascade
);
--> statement-breakpoint
CREATE INDEX `account_userId_idx` ON `account` (`user_id`);--> statement-breakpoint
CREATE TABLE `session` (
	`id` text PRIMARY KEY NOT NULL,
	`expires_at` integer NOT NULL,
	`token` text NOT NULL,
	`created_at` integer DEFAULT (cast(unixepoch('subsecond') * 1000 as integer)) NOT NULL,
	`updated_at` integer NOT NULL,
	`ip_address` text,
	`user_agent` text,
	`user_id` text NOT NULL,
	FOREIGN KEY (`user_id`) REFERENCES `user`(`id`) ON UPDATE no action ON DELETE cascade
);
--> statement-breakpoint
CREATE UNIQUE INDEX `session_token_unique` ON `session` (`token`);--> statement-breakpoint
CREATE INDEX `session_userId_idx` ON `session` (`user_id`);--> statement-breakpoint
CREATE TABLE `user` (
	`id` text PRIMARY KEY NOT NULL,
	`name` text NOT NULL,
	`email` text NOT NULL,
	`email_verified` integer DEFAULT false NOT NULL,
	`image` text,
	`created_at` integer DEFAULT (cast(unixepoch('subsecond') * 1000 as integer)) NOT NULL,
	`updated_at` integer DEFAULT (cast(unixepoch('subsecond') * 1000 as integer)) NOT NULL
);
--> statement-breakpoint
CREATE UNIQUE INDEX `user_email_unique` ON `user` (`email`);--> statement-breakpoint
CREATE TABLE `verification` (
	`id` text PRIMARY KEY NOT NULL,
	`identifier` text NOT NULL,
	`value` text NOT NULL,
	`expires_at` integer NOT NULL,
	`created_at` integer DEFAULT (cast(unixepoch('subsecond') * 1000 as integer)) NOT NULL,
	`updated_at` integer DEFAULT (cast(unixepoch('subsecond') * 1000 as integer)) NOT NULL
);
--> statement-breakpoint
CREATE INDEX `verification_identifier_idx` ON `verification` (`identifier`);--> statement-breakpoint
CREATE TABLE `attendance_config` (
	`id` text PRIMARY KEY NOT NULL,
	`work_start_minute` integer NOT NULL,
	`work_end_minute` integer NOT NULL,
	`off_start_minute` integer NOT NULL,
	`off_end_minute` integer NOT NULL,
	`updated_at` integer DEFAULT (cast(unixepoch('subsecond') * 1000 as integer)) NOT NULL
);
--> statement-breakpoint
CREATE TABLE `attendance_record` (
	`id` text PRIMARY KEY NOT NULL,
	`employee_id` text NOT NULL,
	`device_id` text NOT NULL,
	`recognized_at` integer NOT NULL,
	`local_date` text NOT NULL,
	`type` text NOT NULL,
	`created_at` integer DEFAULT (cast(unixepoch('subsecond') * 1000 as integer)) NOT NULL,
	`updated_at` integer DEFAULT (cast(unixepoch('subsecond') * 1000 as integer)) NOT NULL,
	FOREIGN KEY (`employee_id`) REFERENCES `employee`(`id`) ON UPDATE no action ON DELETE restrict,
	FOREIGN KEY (`device_id`) REFERENCES `device`(`id`) ON UPDATE no action ON DELETE restrict
);
--> statement-breakpoint
CREATE UNIQUE INDEX `attendance_record_employee_date_type_unique_idx` ON `attendance_record` (`employee_id`,`local_date`,`type`);--> statement-breakpoint
CREATE INDEX `attendance_record_local_date_type_idx` ON `attendance_record` (`local_date`,`type`);--> statement-breakpoint
CREATE INDEX `attendance_record_device_local_date_idx` ON `attendance_record` (`device_id`,`local_date`);--> statement-breakpoint
CREATE TABLE `device` (
	`id` text PRIMARY KEY NOT NULL,
	`device_code` text NOT NULL,
	`name` text NOT NULL,
	`api_key_hash` text NOT NULL,
	`status` text DEFAULT 'active' NOT NULL,
	`bootstrap_serial` text,
	`bootstrap_secret_hash` text,
	`activation_status` text DEFAULT 'activated' NOT NULL,
	`last_hello_at` integer,
	`activated_at` integer,
	`last_seen_at` integer,
	`created_at` integer DEFAULT (cast(unixepoch('subsecond') * 1000 as integer)) NOT NULL,
	`updated_at` integer DEFAULT (cast(unixepoch('subsecond') * 1000 as integer)) NOT NULL
);
--> statement-breakpoint
CREATE UNIQUE INDEX `device_code_unique_idx` ON `device` (`device_code`);--> statement-breakpoint
CREATE UNIQUE INDEX `device_bootstrap_serial_unique_idx` ON `device` (`bootstrap_serial`);--> statement-breakpoint
CREATE INDEX `device_activation_status_idx` ON `device` (`activation_status`);--> statement-breakpoint
CREATE TABLE `employee` (
	`id` text PRIMARY KEY NOT NULL,
	`code` text NOT NULL,
	`name` text NOT NULL,
	`created_at` integer DEFAULT (cast(unixepoch('subsecond') * 1000 as integer)) NOT NULL,
	`updated_at` integer DEFAULT (cast(unixepoch('subsecond') * 1000 as integer)) NOT NULL
);
--> statement-breakpoint
CREATE UNIQUE INDEX `employee_code_unique_idx` ON `employee` (`code`);--> statement-breakpoint
CREATE TABLE `face_profile` (
	`id` text PRIMARY KEY NOT NULL,
	`employee_id` text NOT NULL,
	`device_id` text NOT NULL,
	`status` text DEFAULT 'pending' NOT NULL,
	`created_at` integer DEFAULT (cast(unixepoch('subsecond') * 1000 as integer)) NOT NULL,
	`updated_at` integer DEFAULT (cast(unixepoch('subsecond') * 1000 as integer)) NOT NULL,
	FOREIGN KEY (`employee_id`) REFERENCES `employee`(`id`) ON UPDATE no action ON DELETE restrict,
	FOREIGN KEY (`device_id`) REFERENCES `device`(`id`) ON UPDATE no action ON DELETE restrict
);
--> statement-breakpoint
CREATE UNIQUE INDEX `face_profile_employee_unique_idx` ON `face_profile` (`employee_id`);--> statement-breakpoint
CREATE INDEX `face_profile_device_idx` ON `face_profile` (`device_id`);