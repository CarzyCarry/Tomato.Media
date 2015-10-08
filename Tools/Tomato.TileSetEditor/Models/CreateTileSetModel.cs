﻿using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using Catel.Data;
using Tomato.Tools.Common.Gaming;

namespace Tomato.TileSetEditor.Models
{
    class CreateTileSetModel : ModelBase
    {
        public string FileName { get; set; }
        public string Name { get; set; }
        public int TileWidth { get; set; }
        public int TileHeight { get; set; }
        public int TilesPerRow { get; set; }
        public int TilesRowCount { get; set; }
        public int ExtraImageWidth { get; set; }
        public int ExtraImageHeight { get; set; }

        public async Task<TileSetModel> Create()
        {
            var image = new RenderTargetBitmap(TilesPerRow * TileWidth, TilesRowCount * TileHeight, 96, 96, PixelFormats.Pbgra32);
            var extraImage = new RenderTargetBitmap(ExtraImageWidth, ExtraImageHeight, 96, 96, PixelFormats.Pbgra32);
            var tileSet = new TileSetModel(new TileSet
            {
                Name = Name,
                TileWidth = TileWidth,
                TileHeight = TileHeight
            }, FileName, image, extraImage);
            await tileSet.SaveAsync();
            return tileSet;
        }
    }
}
