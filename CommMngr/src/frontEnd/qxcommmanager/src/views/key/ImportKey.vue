<template>
	<el-card shadow="never" header="SM2公钥导入">
		<el-alert title="支持上传PEM格式文件或直接粘贴公钥内容" type="info" show-icon :closable="false" style="margin-bottom:20px"></el-alert>
		<el-form ref="form" :model="form" :rules="rules" label-width="100px">
			<el-form-item label="上传文件">
				<el-upload ref="upload" action="" :auto-upload="false" :show-file-list="true" :on-change="handleFileChange" :limit="1">
					<el-button type="primary" icon="el-icon-upload">选择PEM文件</el-button>
					<template #tip>
						<div class="el-upload__tip">支持 .pem 格式文件</div>
					</template>
				</el-upload>
			</el-form-item>
			<el-divider>或</el-divider>
			<el-form-item label="粘贴公钥" prop="keyContent">
				<el-input type="textarea" :rows="8" v-model="form.keyContent" placeholder="请粘贴SM2公钥内容"></el-input>
			</el-form-item>
			<el-form-item>
				<el-button type="primary" :loading="isSaving" @click="submit">导入</el-button>
				<el-button @click="clear">清空</el-button>
			</el-form-item>
		</el-form>
	</el-card>
</template>

<script>
export default {
	data() {
		return {
			isSaving: false,
			selectedFile: null,
			form: {
				keyContent: ""
			},
			rules: {
				keyContent: [
					{ required: true, message: '请粘贴公钥内容或上传PEM文件' }
				]
			}
		}
	},
	methods: {
		handleFileChange(file) {
			this.selectedFile = file.raw
			var reader = new FileReader()
			reader.onload = (e) => {
				this.form.keyContent = e.target.result
			}
			reader.readAsText(file.raw)
			return false
		},
		async submit() {
			if (!this.form.keyContent && !this.selectedFile) {
				this.$message.warning("请上传文件或粘贴公钥内容")
				return
			}
			this.isSaving = true
			var res = await this.$API.commmngr.keyImport.post({ keyContent: this.form.keyContent })
			this.isSaving = false
			if (res && res.code == 200) {
				this.$message.success("公钥导入成功")
				this.clear()
			} else {
				this.$alert(res.message, "提示", { type: 'error' })
			}
		},
		clear() {
			this.form.keyContent = ""
			this.selectedFile = null
			this.$refs.upload.clearFiles()
		}
	}
}
</script>
