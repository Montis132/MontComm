<template>
	<el-card shadow="never" header="角色注册">
		<el-form ref="form" :model="form" :rules="rules" label-width="120px">
			<el-form-item label="用户名" prop="username">
				<el-input v-model="form.username" placeholder="请输入用户名"></el-input>
			</el-form-item>
			<el-form-item label="密码" prop="password">
				<el-input v-model="form.password" type="password" show-password placeholder="请输入密码"></el-input>
			</el-form-item>
			<el-form-item label="确认密码" prop="password2">
				<el-input v-model="form.password2" type="password" show-password placeholder="请再次输入密码"></el-input>
			</el-form-item>
			<el-form-item label="角色" prop="role">
				<el-radio-group v-model="form.role">
					<el-radio label="partner">合作伙伴</el-radio>
					<el-radio label="user">普通用户</el-radio>
				</el-radio-group>
			</el-form-item>
			<el-form-item v-if="form.role=='partner'" label="服务器名称" prop="serverName">
				<el-input v-model="form.serverName" placeholder="请输入服务器名称"></el-input>
			</el-form-item>
			<el-form-item v-if="form.role=='user'" label="所属组织" prop="organization">
				<el-input v-model="form.organization" placeholder="请输入组织名称"></el-input>
			</el-form-item>
			<el-form-item>
				<el-button type="primary" :loading="isSaving" @click="submit">提交注册</el-button>
				<el-button @click="reset">重置</el-button>
			</el-form-item>
		</el-form>
	</el-card>
</template>

<script>
export default {
	data() {
		return {
			isSaving: false,
			form: {
				username: "",
				password: "",
				password2: "",
				role: "partner",
				serverName: "",
				organization: ""
			},
			rules: {
				username: [
					{ required: true, message: '请输入用户名' }
				],
				password: [
					{ required: true, message: '请输入密码' }
				],
				password2: [
					{ required: true, message: '请再次输入密码' },
					{ validator: (rule, value, callback) => {
						if (value !== this.form.password) {
							callback(new Error('两次输入密码不一致'));
						} else {
							callback();
						}
					}}
				],
				role: [
					{ required: true, message: '请选择角色' }
				],
				serverName: [
					{ required: true, message: '请输入服务器名称' }
				],
				organization: [
					{ required: true, message: '请输入组织名称' }
				]
			}
		}
	},
	methods: {
		async submit() {
			var valid = await this.$refs.form.validate().catch(() => {})
			if (!valid) return
			this.isSaving = true
			var res = await this.$API.commmngr.roleRegister.post(this.form)
			this.isSaving = false
			if (res.code == 200) {
				this.$message.success("注册成功")
				this.reset()
			} else {
				this.$alert(res.message, "提示", { type: 'error' })
			}
		},
		reset() {
			this.$refs.form.resetFields()
		}
	}
}
</script>
