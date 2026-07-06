<template>
	<el-card shadow="never" header="证书导出">
		<el-alert title="点击下方按钮导出当前证书文件" type="info" show-icon :closable="false" style="margin-bottom:20px"></el-alert>
		<el-button type="primary" :loading="isExporting" icon="el-icon-download" @click="exportCert">导出证书</el-button>
	</el-card>
</template>

<script>
export default {
	data() {
		return {
			isExporting: false
		}
	},
	methods: {
		async exportCert() {
			this.isExporting = true
			var res = await this.$API.commmngr.certExport.get()
			this.isExporting = false
			if (res && res.code == 200) {
				var link = document.createElement('a')
				link.href = res.data
				link.setAttribute('download', 'certificate.crt')
				document.body.appendChild(link)
				link.click()
				document.body.removeChild(link)
				this.$message.success("证书导出成功")
			} else {
				this.$message.error("证书导出失败")
			}
		}
	}
}
</script>
