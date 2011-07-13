// D3D11TextureCube.cpp
// KlayGE D3D11 Cube纹理类 实现文件
// Ver 3.8.0
// 版权所有(C) 龚敏敏, 2009
// Homepage: http://www.klayge.org
//
// 3.8.0
// 初次建立 (2009.1.30)
//
// 修改记录
/////////////////////////////////////////////////////////////////////////////////

#include <KlayGE/KlayGE.hpp>
#include <KlayGE/Util.hpp>
#include <KlayGE/COMPtr.hpp>
#include <KlayGE/ThrowErr.hpp>
#include <KlayGE/Math.hpp>
#include <KlayGE/Context.hpp>
#include <KlayGE/RenderEngine.hpp>
#include <KlayGE/RenderFactory.hpp>
#include <KlayGE/Math.hpp>
#include <KlayGE/Texture.hpp>

#include <cstring>

#include <KlayGE/D3D11/D3D11MinGWDefs.hpp>
#include <d3d11.h>
#include <d3dx11.h>

#include <KlayGE/D3D11/D3D11Typedefs.hpp>
#include <KlayGE/D3D11/D3D11RenderEngine.hpp>
#include <KlayGE/D3D11/D3D11Mapping.hpp>
#include <KlayGE/D3D11/D3D11Texture.hpp>

#ifdef KLAYGE_COMPILER_MSVC
#ifdef KLAYGE_DEBUG
	#pragma comment(lib, "d3dx11d.lib")
#else
	#pragma comment(lib, "d3dx11.lib")
#endif
#endif

namespace KlayGE
{
	D3D11TextureCube::D3D11TextureCube(uint32_t size, uint32_t numMipMaps, uint32_t array_size, ElementFormat format,
						uint32_t sample_count, uint32_t sample_quality, uint32_t access_hint, ElementInitData* init_data)
					: D3D11Texture(TT_Cube, sample_count, sample_quality, access_hint)
	{
		if (0 == numMipMaps)
		{
			num_mip_maps_ = 1;
			uint32_t w = size;
			while (w != 1)
			{
				++ num_mip_maps_;

				w = std::max<uint32_t>(1U, w / 2);
			}
		}
		else
		{
			num_mip_maps_ = numMipMaps;
		}

		D3D11RenderEngine const & re = *checked_cast<D3D11RenderEngine const *>(&Context::Instance().RenderFactoryInstance().RenderEngineInstance());
		if (re.DeviceFeatureLevel() <= D3D_FEATURE_LEVEL_9_3)
		{
			if ((num_mip_maps_ > 1) && ((size & (size - 1)) != 0))
			{
				// height or width is not a power of 2 and multiple mip levels are specified. This is not supported at feature levels below 10.0.
				num_mip_maps_ = 1;
			}
		}

		array_size_ = array_size;
		format_		= format;
		widthes_.assign(1, size);

		desc_.Width = size;
		desc_.Height = size;
		desc_.MipLevels = num_mip_maps_;
		desc_.ArraySize = 6 * array_size_;
		desc_.Format = D3D11Mapping::MappingFormat(format_);
		desc_.SampleDesc.Count = 1;
		desc_.SampleDesc.Quality = 0;

		this->GetD3DFlags(desc_.Usage, desc_.BindFlags, desc_.CPUAccessFlags, desc_.MiscFlags);
		desc_.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;

		std::vector<D3D11_SUBRESOURCE_DATA> subres_data(6 * num_mip_maps_);
		if (init_data != NULL)
		{
			for (int face = 0; face < 6; ++ face)
			{
				for (uint32_t i = 0; i < num_mip_maps_; ++ i)
				{
					subres_data[face * num_mip_maps_ + i].pSysMem = init_data[face * num_mip_maps_ + i].data;
					subres_data[face * num_mip_maps_ + i].SysMemPitch = init_data[face * num_mip_maps_ + i].row_pitch;
					subres_data[face * num_mip_maps_ + i].SysMemSlicePitch = init_data[face * num_mip_maps_ + i].slice_pitch;
				}
			}
		}

		ID3D11Texture2D* d3d_tex;
		TIF(d3d_device_->CreateTexture2D(&desc_, (init_data != NULL) ? &subres_data[0] : NULL, &d3d_tex));
		d3dTextureCube_ = MakeCOMPtr(d3d_tex);

		this->UpdateParams();
	}

	uint32_t D3D11TextureCube::Width(uint32_t level) const
	{
		BOOST_ASSERT(level < num_mip_maps_);

		return widthes_[level];
	}

	uint32_t D3D11TextureCube::Height(uint32_t level) const
	{
		return this->Width(level);
	}

	void D3D11TextureCube::CopyToTexture(Texture& target)
	{
		BOOST_ASSERT(type_ == target.Type());

		D3D11TextureCube& other(*checked_cast<D3D11TextureCube*>(&target));

		if ((this->Width(0) == target.Width(0)) && (this->Format() == target.Format()) && (this->NumMipMaps() == target.NumMipMaps()))
		{
			d3d_imm_ctx_->CopyResource(other.D3DTexture().get(), d3dTextureCube_.get());
		}
		else
		{
			D3DX11_TEXTURE_LOAD_INFO info;
			info.pSrcBox = NULL;
			info.pDstBox = NULL;
			info.NumMips = std::min(this->NumMipMaps(), target.NumMipMaps());
			info.SrcFirstElement = 0;
			info.DstFirstElement = 0;
			info.NumElements = 0;
			info.Filter = D3DX11_FILTER_LINEAR;
			info.MipFilter = D3DX11_FILTER_LINEAR;
			if (IsSRGB(format_))
			{
				info.Filter |= D3DX11_FILTER_SRGB_IN;
				info.MipFilter |= D3DX11_FILTER_SRGB_IN;
			}
			if (IsSRGB(target.Format()))
			{
				info.Filter |= D3DX11_FILTER_SRGB_OUT;
				info.MipFilter |= D3DX11_FILTER_SRGB_OUT;
			}

			for (int face = 0; face < 6; ++ face)
			{
				info.SrcFirstMip = D3D11CalcSubresource(0, face, this->NumMipMaps());
				info.DstFirstMip = D3D11CalcSubresource(0, face, other.NumMipMaps());
				D3DX11LoadTextureFromTexture(d3d_imm_ctx_.get(), d3dTextureCube_.get(), &info, other.D3DTexture().get());
			}
		}
	}

	void D3D11TextureCube::CopyToSubTextureCube(Texture& target,
			uint32_t dst_array_index, CubeFaces dst_face, uint32_t dst_level, uint32_t dst_x_offset, uint32_t dst_y_offset, uint32_t dst_width, uint32_t dst_height,
			uint32_t src_array_index, CubeFaces src_face, uint32_t src_level, uint32_t src_x_offset, uint32_t src_y_offset, uint32_t src_width, uint32_t src_height)
	{
		BOOST_ASSERT(type_ == target.Type());

		this->CopyToSubTexture(target,
			D3D11CalcSubresource(dst_level, dst_array_index * 6 + dst_face - CF_Positive_X, target.NumMipMaps()), dst_x_offset, dst_y_offset, 0, dst_width, dst_height, 1,
			D3D11CalcSubresource(src_level, src_array_index * 6 + src_face - CF_Positive_X, this->NumMipMaps()), src_x_offset, src_y_offset, 0, src_width, src_height, 1);
	}

	ID3D11ShaderResourceViewPtr const & D3D11TextureCube::RetriveD3DShaderResourceView(uint32_t first_array_index, uint32_t num_items, uint32_t first_level, uint32_t num_levels)
	{
		BOOST_ASSERT(this->AccessHint() & EAH_GPU_Read);
		BOOST_ASSERT(0 == first_array_index);
		BOOST_ASSERT(1 == num_items);
		UNREF_PARAM(first_array_index);
		UNREF_PARAM(num_items);

		D3D11_SHADER_RESOURCE_VIEW_DESC sr_desc;
		switch (format_)
		{
		case EF_D16:
			sr_desc.Format = DXGI_FORMAT_R16_UNORM;
			break;

		case EF_D24S8:
			sr_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			break;

		case EF_D32F:
			sr_desc.Format = DXGI_FORMAT_R32_FLOAT;
			break;

		default:
			sr_desc.Format = desc_.Format;
			break;
		}

		sr_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURECUBE;
		sr_desc.TextureCube.MostDetailedMip = first_level;
		sr_desc.TextureCube.MipLevels = num_levels;

		for (size_t i = 0; i < d3d_sr_views_.size(); ++ i)
		{
			if (0 == memcmp(&d3d_sr_views_[i].first, &sr_desc, sizeof(sr_desc)))
			{
				return d3d_sr_views_[i].second;
			}
		}

		ID3D11ShaderResourceView* d3d_sr_view;
		d3d_device_->CreateShaderResourceView(this->D3DTexture().get(), &sr_desc, &d3d_sr_view);
		d3d_sr_views_.push_back(std::make_pair(sr_desc, MakeCOMPtr(d3d_sr_view)));
		return d3d_sr_views_.back().second;
	}

	ID3D11UnorderedAccessViewPtr const & D3D11TextureCube::RetriveD3DUnorderedAccessView(uint32_t first_array_index, uint32_t num_items, uint32_t level)
	{
		return this->RetriveD3DUnorderedAccessView(first_array_index, num_items, CF_Positive_X, 6, level);
	}

	ID3D11UnorderedAccessViewPtr const & D3D11TextureCube::RetriveD3DUnorderedAccessView(uint32_t first_array_index, uint32_t num_items, CubeFaces first_face, uint32_t num_faces, uint32_t level)
	{
		BOOST_ASSERT(this->AccessHint() & EAH_GPU_Read);

		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		uav_desc.Format = desc_.Format;
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		uav_desc.Texture2DArray.MipSlice = level;
		uav_desc.Texture2DArray.ArraySize = num_items * 6 + num_faces;
		uav_desc.Texture2DArray.FirstArraySlice = first_array_index * 6 + first_face;

		for (size_t i = 0; i < d3d_ua_views_.size(); ++ i)
		{
			if (0 == memcmp(&d3d_ua_views_[i].first, &uav_desc, sizeof(uav_desc)))
			{
				return d3d_ua_views_[i].second;
			}
		}

		ID3D11UnorderedAccessView* d3d_ua_view;
		d3d_device_->CreateUnorderedAccessView(this->D3DTexture().get(), &uav_desc, &d3d_ua_view);
		d3d_ua_views_.push_back(std::make_pair(uav_desc, MakeCOMPtr(d3d_ua_view)));
		return d3d_ua_views_.back().second;
	}

	ID3D11RenderTargetViewPtr const & D3D11TextureCube::RetriveD3DRenderTargetView(uint32_t array_index, uint32_t level)
	{
		BOOST_ASSERT(this->AccessHint() & EAH_GPU_Write);

		RTVDSVCreation rtv_creation;
		memset(&rtv_creation, 0, sizeof(rtv_creation));
		rtv_creation.array_index = array_index;
		rtv_creation.level = level;
		for (size_t i = 0; i < d3d_rt_views_.size(); ++ i)
		{
			if (0 == memcmp(&d3d_rt_views_[i].first, &rtv_creation, sizeof(rtv_creation)))
			{
				return d3d_rt_views_[i].second;
			}
		}

		D3D11_RENDER_TARGET_VIEW_DESC desc;
		desc.Format = D3D11Mapping::MappingFormat(this->Format());

		if (this->SampleCount() > 1)
		{
			desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
		}
		else
		{
			desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		}
		desc.Texture2DArray.MipSlice = level;
		desc.Texture2DArray.ArraySize = 6;
		desc.Texture2DArray.FirstArraySlice = array_index * 6;

		ID3D11RenderTargetView* rt_view;
		TIF(d3d_device_->CreateRenderTargetView(this->D3DTexture().get(), &desc, &rt_view));
		d3d_rt_views_.push_back(std::make_pair(rtv_creation, MakeCOMPtr(rt_view)));
		return d3d_rt_views_.back().second;
	}

	ID3D11RenderTargetViewPtr const & D3D11TextureCube::RetriveD3DRenderTargetView(uint32_t array_index, Texture::CubeFaces face, uint32_t level)
	{
		BOOST_ASSERT(this->AccessHint() & EAH_GPU_Write);

		RTVDSVCreation rtv_creation;
		memset(&rtv_creation, 0, sizeof(rtv_creation));
		rtv_creation.array_index = array_index;
		rtv_creation.level = level;
		rtv_creation.for_3d_or_cube.for_cube.face = face;
		for (size_t i = 0; i < d3d_rt_views_.size(); ++ i)
		{
			if (0 == memcmp(&d3d_rt_views_[i].first, &rtv_creation, sizeof(rtv_creation)))
			{
				return d3d_rt_views_[i].second;
			}
		}

		D3D11_RENDER_TARGET_VIEW_DESC desc;
		desc.Format = D3D11Mapping::MappingFormat(this->Format());
		if (this->SampleCount() > 1)
		{
			desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
		}
		else
		{
			desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		}
		desc.Texture2DArray.MipSlice = level;
		desc.Texture2DArray.FirstArraySlice = array_index * 6 + face - CF_Positive_X;
		desc.Texture2DArray.ArraySize = 1;

		ID3D11RenderTargetView* rt_view;
		TIF(d3d_device_->CreateRenderTargetView(this->D3DTexture().get(), &desc, &rt_view));
		d3d_rt_views_.push_back(std::make_pair(rtv_creation, MakeCOMPtr(rt_view)));
		return d3d_rt_views_.back().second;
	}

	ID3D11DepthStencilViewPtr const & D3D11TextureCube::RetriveD3DDepthStencilView(uint32_t array_index, uint32_t level)
	{
		BOOST_ASSERT(this->AccessHint() & EAH_GPU_Write);

		RTVDSVCreation dsv_creation;
		memset(&dsv_creation, 0, sizeof(dsv_creation));
		dsv_creation.array_index = array_index;
		dsv_creation.level = level;
		for (size_t i = 0; i < d3d_ds_views_.size(); ++ i)
		{
			if (0 == memcmp(&d3d_ds_views_[i].first, &dsv_creation, sizeof(dsv_creation)))
			{
				return d3d_ds_views_[i].second;
			}
		}

		D3D11_DEPTH_STENCIL_VIEW_DESC desc;
		desc.Format = D3D11Mapping::MappingFormat(this->Format());
		desc.Flags = 0;

		if (this->SampleCount() > 1)
		{
			desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
		}
		else
		{
			desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		}
		desc.Texture2DArray.MipSlice = level;
		desc.Texture2DArray.ArraySize = 6;
		desc.Texture2DArray.FirstArraySlice = array_index * 6;

		ID3D11DepthStencilView* ds_view;
		TIF(d3d_device_->CreateDepthStencilView(this->D3DTexture().get(), &desc, &ds_view));
		d3d_ds_views_.push_back(std::make_pair(dsv_creation, MakeCOMPtr(ds_view)));
		return d3d_ds_views_.back().second;
	}

	ID3D11DepthStencilViewPtr const & D3D11TextureCube::RetriveD3DDepthStencilView(uint32_t array_index, Texture::CubeFaces face, uint32_t level)
	{
		BOOST_ASSERT(this->AccessHint() & EAH_GPU_Write);

		RTVDSVCreation dsv_creation;
		memset(&dsv_creation, 0, sizeof(dsv_creation));
		dsv_creation.array_index = array_index;
		dsv_creation.level = level;
		for (size_t i = 0; i < d3d_ds_views_.size(); ++ i)
		{
			if (0 == memcmp(&d3d_ds_views_[i].first, &dsv_creation, sizeof(dsv_creation)))
			{
				return d3d_ds_views_[i].second;
			}
		}

		D3D11_DEPTH_STENCIL_VIEW_DESC desc;
		desc.Format = D3D11Mapping::MappingFormat(this->Format());
		desc.Flags = 0;

		if (this->SampleCount() > 1)
		{
			desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
		}
		else
		{
			desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		}
		desc.Texture2DArray.MipSlice = level;
		desc.Texture2DArray.ArraySize = 1;
		desc.Texture2DArray.FirstArraySlice = array_index * 6 + face - CF_Positive_X;

		ID3D11DepthStencilView* ds_view;
		TIF(d3d_device_->CreateDepthStencilView(this->D3DTexture().get(), &desc, &ds_view));
		d3d_ds_views_.push_back(std::make_pair(dsv_creation, MakeCOMPtr(ds_view)));
		return d3d_ds_views_.back().second;
	}

	void D3D11TextureCube::MapCube(uint32_t array_index, CubeFaces face, uint32_t level, TextureMapAccess tma,
			uint32_t x_offset, uint32_t y_offset, uint32_t /*width*/, uint32_t /*height*/,
			void*& data, uint32_t& row_pitch)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		TIF(d3d_imm_ctx_->Map(d3dTextureCube_.get(), D3D11CalcSubresource(level, array_index * 6 + face - CF_Positive_X, num_mip_maps_),
			D3D11Mapping::Mapping(tma, type_, access_hint_, num_mip_maps_), 0, &mapped));
		uint8_t* p = static_cast<uint8_t*>(mapped.pData);
		data = p + y_offset * mapped.RowPitch + x_offset * NumFormatBytes(format_);
		row_pitch = mapped.RowPitch;
	}

	void D3D11TextureCube::UnmapCube(uint32_t array_index, CubeFaces face, uint32_t level)
	{
		d3d_imm_ctx_->Unmap(d3dTextureCube_.get(), D3D11CalcSubresource(level, array_index * 6 + face - CF_Positive_X, num_mip_maps_));
	}

	void D3D11TextureCube::BuildMipSubLevels()
	{
		if (!d3d_sr_views_.empty())
		{
			BOOST_ASSERT(access_hint_ & EAH_Generate_Mips);
			d3d_imm_ctx_->GenerateMips(d3d_sr_views_[0].second.get());
		}
		else
		{
			D3DX11FilterTexture(d3d_imm_ctx_.get(), d3dTextureCube_.get(), 0, D3DX11_FILTER_LINEAR);
		}
	}

	void D3D11TextureCube::UpdateParams()
	{
		d3dTextureCube_->GetDesc(&desc_);

		num_mip_maps_ = desc_.MipLevels;
		array_size_ = desc_.ArraySize / 6;
		BOOST_ASSERT(num_mip_maps_ != 0);

		widthes_.resize(num_mip_maps_);
		widthes_[0] = desc_.Width;
		for (uint32_t level = 1; level < num_mip_maps_; ++ level)
		{
			widthes_[level] = std::max<uint32_t>(1U, widthes_[level - 1] / 2);
		}

		format_ = D3D11Mapping::MappingFormat(desc_.Format);
	}
}
