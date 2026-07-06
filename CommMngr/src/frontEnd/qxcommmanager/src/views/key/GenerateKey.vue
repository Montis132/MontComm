<template>
	<el-card shadow="never" header="密钥生成">
		<el-alert title="点击生成按钮将生成新的密钥对，公钥将显示在下方区域" type="info" show-icon :closable="false" style="margin-bottom:20px"></el-alert>
		<el-button type="primary" :loading="isGenerating" icon="el-icon-refresh" @click="generateKey">生成密钥</el-button>
		<el-card v-if="publicKey" shadow="never" style="margin-top:20px">
			<template #header>
				<span>生成的公钥</span>
				<el-button style="float:right" size="small" type="primary" plain @click="copyKey">复制</el-button>
			</template>
			<el-input type="textarea" :rows="6" v-model="publicKey" readonly></el-input>
		</el-card>
	</el-card>
</template>

<script>
export default {
	data() {
		return {
			isGenerating: false,
			publicKey: ""
		}
	},
	methods: {
		async generateKey() {
			this.isGenerating = true
			var res = await this.$API.commmngr.keyGenerate.post()
			this.isGenerating = false
			if (res && res.code == 200) {
				this.publicKey = res.data.publicKey
				this.$message.success("密钥生成成功")
			} else {
				this.$message.error("密钥生成失败")
			}
		},
		copyKey() {
			navigator.clipboard.writeText(this.publicKey).then(() => {
				this.$message.success("已复制到剪贴板")
			})
		}
	}
}
</script>
